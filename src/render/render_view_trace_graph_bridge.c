#include "render/render_view_internal.h"

#include <math.h>
#include <string.h>

#include "kit_graph_timeseries.h"

static int trace_graph_round_float(float value) {
    if (value >= 0.0f) {
        return (int)(value + 0.5f);
    }
    return (int)(value - 0.5f);
}

static SDL_Rect trace_graph_sdl_rect(KitRenderRect rect) {
    SDL_Rect out;
    out.x = trace_graph_round_float(rect.x);
    out.y = trace_graph_round_float(rect.y);
    out.w = trace_graph_round_float(rect.width);
    out.h = trace_graph_round_float(rect.height);
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static void trace_graph_set_color(SDL_Renderer *renderer, KitRenderColor color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void trace_graph_draw_command(SDL_Renderer *renderer, const KitRenderCommand *cmd) {
    SDL_Rect rect;
    if (!renderer || !cmd) {
        return;
    }

    switch (cmd->kind) {
        case KIT_RENDER_CMD_SET_CLIP:
            rect = trace_graph_sdl_rect(cmd->data.clip.rect);
            SDL_RenderSetClipRect(renderer, &rect);
            break;
        case KIT_RENDER_CMD_CLEAR_CLIP:
            SDL_RenderSetClipRect(renderer, NULL);
            break;
        case KIT_RENDER_CMD_RECT:
            rect = trace_graph_sdl_rect(cmd->data.rect.rect);
            trace_graph_set_color(renderer, cmd->data.rect.color);
            SDL_RenderFillRect(renderer, &rect);
            break;
        case KIT_RENDER_CMD_LINE:
            trace_graph_set_color(renderer, cmd->data.line.color);
            SDL_RenderDrawLine(renderer,
                               trace_graph_round_float(cmd->data.line.p0.x),
                               trace_graph_round_float(cmd->data.line.p0.y),
                               trace_graph_round_float(cmd->data.line.p1.x),
                               trace_graph_round_float(cmd->data.line.p1.y));
            break;
        case KIT_RENDER_CMD_POLYLINE:
            trace_graph_set_color(renderer, cmd->data.polyline.color);
            for (uint32_t i = 1u; i < cmd->data.polyline.point_count; ++i) {
                const KitRenderVec2 *prev = &cmd->data.polyline.points[i - 1u];
                const KitRenderVec2 *cur = &cmd->data.polyline.points[i];
                SDL_RenderDrawLine(renderer,
                                   trace_graph_round_float(prev->x),
                                   trace_graph_round_float(prev->y),
                                   trace_graph_round_float(cur->x),
                                   trace_graph_round_float(cur->y));
            }
            break;
        case KIT_RENDER_CMD_TEXT:
            draw_text_5x7(renderer,
                          trace_graph_round_float(cmd->data.text.origin.x),
                          trace_graph_round_float(cmd->data.text.origin.y),
                          cmd->data.text.text ? cmd->data.text.text : "",
                          1,
                          205,
                          210,
                          220,
                          255);
            break;
        case KIT_RENDER_CMD_CLEAR:
        case KIT_RENDER_CMD_TEXTURED_QUAD:
        default:
            break;
    }
}

static void trace_graph_replay(SDL_Renderer *renderer, const KitRenderCommandBuffer *buffer) {
    if (!renderer || !buffer || !buffer->commands) {
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (size_t i = 0u; i < buffer->count; ++i) {
        trace_graph_draw_command(renderer, &buffer->commands[i]);
    }
    SDL_RenderSetClipRect(renderer, NULL);
}

static int trace_graph_project_point(const KitGraphTsView *view,
                                     KitRenderRect bounds,
                                     const KitGraphTsStyle *style,
                                     float sample_x,
                                     float sample_y,
                                     float *out_x,
                                     float *out_y) {
    KitRenderRect inner;
    float tx;
    float ty;
    if (!view || !style || !out_x || !out_y || !(view->x_max > view->x_min) || !(view->y_max > view->y_min)) {
        return 0;
    }
    inner.x = bounds.x + style->padding;
    inner.y = bounds.y + style->padding;
    inner.width = bounds.width - (style->padding * 2.0f);
    inner.height = bounds.height - (style->padding * 2.0f);
    if (inner.width <= 0.0f || inner.height <= 0.0f) {
        return 0;
    }
    tx = (sample_x - view->x_min) / (view->x_max - view->x_min);
    ty = (sample_y - view->y_min) / (view->y_max - view->y_min);
    if (tx < 0.0f) tx = 0.0f;
    if (tx > 1.0f) tx = 1.0f;
    if (ty < 0.0f) ty = 0.0f;
    if (ty > 1.0f) ty = 1.0f;
    *out_x = inner.x + (tx * inner.width);
    *out_y = inner.y + inner.height - (ty * inner.height);
    return 1;
}

CoreResult datalab_trace_graph_draw_shared(SDL_Renderer *renderer,
                                           int frame_width,
                                           int frame_height,
                                           const SDL_Rect *band,
                                           const KitGraphTsSeries *series,
                                           float zoom_factor,
                                           int inspect_active,
                                           float inspect_x,
                                           float inspect_y,
                                           KitGraphTsHover *out_hover) {
    enum { DATALAB_TRACE_GRAPH_COMMAND_CAP = 2048 };
    KitRenderCommand commands[DATALAB_TRACE_GRAPH_COMMAND_CAP];
    KitRenderCommandBuffer buffer;
    KitRenderFrame kit_frame;
    KitRenderRect bounds;
    KitGraphTsStyle style;
    KitGraphTsView view;
    CoreResult result;
    float hover_screen_x = 0.0f;
    float hover_screen_y = 0.0f;

    if (out_hover) {
        memset(out_hover, 0, sizeof(*out_hover));
    }
    if (!renderer || !band || !series || !series->xs || !series->ys || series->point_count == 0u ||
        frame_width <= 0 || frame_height <= 0 || band->w <= 0 || band->h <= 0) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid DataLab trace graph draw request" };
    }

    bounds.x = (float)band->x;
    bounds.y = (float)band->y;
    bounds.width = (float)band->w;
    bounds.height = (float)band->h;

    kit_graph_ts_style_default(&style);
    style.show_axes = 1;
    style.show_grid = 1;
    style.show_legend = 0;
    style.show_hover_crosshair = 1;
    style.show_hover_label = 0;
    style.grid_x_divisions = 8u;
    style.grid_y_divisions = 4u;
    style.max_render_points = 0u;
    style.line_thickness = 1.0f;
    style.hover_radius = 3.0f;

    result = kit_graph_ts_compute_view(series, 1u, &view);
    if (result.code != CORE_OK) {
        return result;
    }
    if (isfinite(zoom_factor) && zoom_factor > 0.0f && fabsf(zoom_factor - 1.0f) > 1e-5f) {
        result = kit_graph_ts_zoom_view(&view, zoom_factor);
        if (result.code != CORE_OK) {
            return result;
        }
    }

    buffer.commands = commands;
    buffer.capacity = DATALAB_TRACE_GRAPH_COMMAND_CAP;
    buffer.count = 0u;
    kit_frame.width_px = (uint32_t)frame_width;
    kit_frame.height_px = (uint32_t)frame_height;
    kit_frame.command_buffer = &buffer;

    result = kit_graph_ts_draw_plot(&kit_frame, bounds, series, 1u, &view, &style);
    if (result.code != CORE_OK) {
        return result;
    }

    if (inspect_active &&
        trace_graph_project_point(&view, bounds, &style, inspect_x, inspect_y, &hover_screen_x, &hover_screen_y)) {
        KitGraphTsHover hover;
        result = kit_graph_ts_hover_inspect(series, &view, bounds, hover_screen_x, hover_screen_y, &hover);
        if (result.code != CORE_OK) {
            return result;
        }
        if (out_hover) {
            *out_hover = hover;
        }
        result = kit_graph_ts_draw_hover_overlay(&kit_frame, bounds, &view, &style, &hover);
        if (result.code != CORE_OK) {
            return result;
        }
    }

    trace_graph_replay(renderer, &buffer);
    return core_result_ok();
}
