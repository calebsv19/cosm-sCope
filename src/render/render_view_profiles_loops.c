#include "render/render_view_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ui/input.h"

typedef struct DatalabPhysicsLoopContext {
    SDL_Texture *texture;
    uint8_t *density_rgba;
    uint8_t *speed_rgba;
    KitVizVecSegment *segments;
    size_t sample_count;
} DatalabPhysicsLoopContext;

typedef struct DatalabSketchLoopContext {
    DatalabRasterTextureState *texture_state;
} DatalabSketchLoopContext;

static CoreResult datalab_loop_render_step_physics(SDL_Window *window,
                                                   SDL_Renderer *renderer,
                                                   const DatalabFrame *frame,
                                                   DatalabAppState *app_state,
                                                   void *lane_ctx,
                                                   DatalabRenderSubmitOutcome *out_submit) {
    DatalabPhysicsLoopContext *ctx = (DatalabPhysicsLoopContext *)lane_ctx;
    DatalabPhysicsRenderDeriveFrame render_derive;
    if (!ctx || !out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid physics loop context" };
    }
    datalab_physics_render_derive_frame(renderer,
                                        frame,
                                        app_state,
                                        ctx->density_rgba,
                                        ctx->speed_rgba,
                                        &render_derive);
    datalab_physics_render_submit_frame(window,
                                        renderer,
                                        ctx->texture,
                                        frame,
                                        app_state,
                                        ctx->segments,
                                        ctx->sample_count,
                                        &render_derive,
                                        out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_daw(SDL_Window *window,
                                               SDL_Renderer *renderer,
                                               const DatalabFrame *frame,
                                               DatalabAppState *app_state,
                                               void *lane_ctx,
                                               DatalabRenderSubmitOutcome *out_submit) {
    DatalabRenderDeriveFrame render_derive;
    (void)lane_ctx;
    if (!out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid daw loop outcome" };
    }
    datalab_render_derive_frame(frame, app_state, &render_derive);
    datalab_daw_render_submit_frame(window, renderer, frame, app_state, &render_derive, out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_trace(SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 const DatalabFrame *frame,
                                                 DatalabAppState *app_state,
                                                 void *lane_ctx,
                                                 DatalabRenderSubmitOutcome *out_submit) {
    DatalabRenderDeriveFrame render_derive;
    (void)lane_ctx;
    if (!out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid trace loop outcome" };
    }
    datalab_render_derive_frame(frame, app_state, &render_derive);
    datalab_trace_render_submit_frame(window, renderer, frame, app_state, &render_derive, out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_sketch(SDL_Window *window,
                                                  SDL_Renderer *renderer,
                                                  const DatalabFrame *frame,
                                                  DatalabAppState *app_state,
                                                  void *lane_ctx,
                                                  DatalabRenderSubmitOutcome *out_submit) {
    DatalabSketchLoopContext *ctx = (DatalabSketchLoopContext *)lane_ctx;
    DatalabSketchRenderDeriveFrame render_derive;
    if (!ctx || !out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid sketch loop context" };
    }
    datalab_sketch_render_derive_frame(renderer, frame, app_state, &render_derive);
    datalab_sketch_render_submit_frame(window,
                                       renderer,
                                       ctx->texture_state,
                                       frame,
                                       app_state,
                                       &render_derive,
                                       out_submit);
    return out_submit->result;
}

CoreResult render_physics_loop(SDL_Window *window,
                               SDL_Renderer *renderer,
                               const DatalabFrame *frame,
                               DatalabAppState *app_state) {
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             (int)frame->width,
                                             (int)frame->height);
    const size_t sample_count = (size_t)frame->width * (size_t)frame->height;
    const size_t rgba_size = sample_count * 4u;
    uint8_t *density_rgba = NULL;
    uint8_t *speed_rgba = NULL;
    float *speed = NULL;
    KitVizVecSegment *segments = NULL;
    DatalabPhysicsLoopContext physics_ctx;
    DatalabLoopProfileOps ops;
    CoreResult loop_result = core_result_ok();
    if (!texture) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }

    density_rgba = (uint8_t *)core_alloc(rgba_size);
    speed_rgba = (uint8_t *)core_alloc(rgba_size);
    speed = (float *)core_alloc(sample_count * sizeof(float));
    segments = (KitVizVecSegment *)core_alloc(sample_count * sizeof(KitVizVecSegment));
    if (!density_rgba || !speed_rgba || !speed || !segments) {
        loop_result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }

    {
        KitVizFieldStats dens_stats;
        CoreResult rs = kit_viz_compute_field_stats(frame->density, frame->width, frame->height, &dens_stats);
        if (rs.code != CORE_OK) {
            loop_result = rs;
            goto cleanup;
        }
        for (size_t i = 0; i < sample_count; ++i) {
            const float x = frame->velx[i];
            const float y = frame->vely[i];
            speed[i] = sqrtf((x * x) + (y * y));
        }
        {
            KitVizFieldStats speed_stats;
            rs = kit_viz_compute_field_stats(speed, frame->width, frame->height, &speed_stats);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
            rs = kit_viz_build_heatmap_rgba(frame->density,
                                            frame->width,
                                            frame->height,
                                            dens_stats.min_value,
                                            dens_stats.max_value,
                                            KIT_VIZ_COLORMAP_HEAT,
                                            density_rgba,
                                            rgba_size);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
            rs = kit_viz_build_heatmap_rgba(speed,
                                            frame->width,
                                            frame->height,
                                            speed_stats.min_value,
                                            speed_stats.max_value,
                                            KIT_VIZ_COLORMAP_HEAT,
                                            speed_rgba,
                                            rgba_size);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
        }
    }

    physics_ctx.texture = texture;
    physics_ctx.density_rgba = density_rgba;
    physics_ctx.speed_rgba = speed_rgba;
    physics_ctx.segments = segments;
    physics_ctx.sample_count = sample_count;

    ops.lane_tag = "physics";
    ops.lane_ctx = &physics_ctx;
    ops.render_step = datalab_loop_render_step_physics;
    loop_result = datalab_loop_run_profile(window, renderer, frame, app_state, &ops);

cleanup:
    core_free(density_rgba);
    core_free(speed_rgba);
    core_free(speed);
    core_free(segments);
    SDL_DestroyTexture(texture);
    return loop_result;
}

CoreResult render_volume_loop(SDL_Window *window,
                              SDL_Renderer *renderer,
                              const DatalabFrame *frame,
                              DatalabAppState *app_state) {
    if (!frame || frame->profile != DATALAB_PROFILE_VOLUME || frame->volume_depth == 0u ||
        frame->volume_slice_index >= frame->volume_depth) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid volume slice frame" };
    }
    return render_physics_loop(window, renderer, frame, app_state);
}

CoreResult render_daw_loop(SDL_Window *window,
                           SDL_Renderer *renderer,
                           const DatalabFrame *frame,
                           DatalabAppState *app_state) {
    const DatalabLoopProfileOps ops = {
        .lane_tag = "daw",
        .lane_ctx = NULL,
        .render_step = datalab_loop_render_step_daw
    };
    return datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
}

CoreResult render_sketch_loop(SDL_Window *window,
                              SDL_Renderer *renderer,
                              const DatalabFrame *frame,
                              DatalabAppState *app_state,
                              DatalabRasterTextureState *texture_state) {
    DatalabSketchLoopContext sketch_ctx;
    DatalabLoopProfileOps ops;
    CoreResult loop_result = core_result_ok();
    if (!texture_state) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "missing sketch texture state" };
    }
    memset(&sketch_ctx, 0, sizeof(sketch_ctx));
    sketch_ctx.texture_state = texture_state;
    ops.lane_tag = "sketch";
    ops.lane_ctx = &sketch_ctx;
    ops.render_step = datalab_loop_render_step_sketch;
    loop_result = datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
    return loop_result;
}

CoreResult render_trace_loop(SDL_Window *window,
                             SDL_Renderer *renderer,
                             const DatalabFrame *frame,
                             DatalabAppState *app_state) {
    const DatalabLoopProfileOps ops = {
        .lane_tag = "trace",
        .lane_ctx = NULL,
        .render_step = datalab_loop_render_step_trace
    };
    return datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
}

static CoreResult datalab_line_diagnostic_draw(SDL_Renderer *renderer,
                                               const DatalabFrame *frame,
                                               DatalabRenderSubmitOutcome *outcome) {
    int width = 0, height = 0;
    float min_x, max_x, min_y, max_y, span_x, span_y, scale, ox, oy;
    if (!renderer || !frame || !outcome || !frame->line_anchors || frame->line_anchor_count == 0u) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "line diagnostic geometry unavailable" };
    }
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    min_x = max_x = frame->line_anchors[0].x;
    min_y = max_y = frame->line_anchors[0].y;
    for (uint32_t i = 1u; i < frame->line_anchor_count; ++i) {
        const DatalabLineAnchor *a = &frame->line_anchors[i];
        if (a->x < min_x) min_x = a->x; if (a->x > max_x) max_x = a->x;
        if (a->y < min_y) min_y = a->y; if (a->y > max_y) max_y = a->y;
    }
    span_x = max_x - min_x; span_y = max_y - min_y;
    if (span_x < 0.001f) span_x = 1.0f; if (span_y < 0.001f) span_y = 1.0f;
    scale = fminf(((float)width * 0.82f) / span_x, ((float)height * 0.82f) / span_y);
    ox = ((float)width - span_x * scale) * 0.5f - min_x * scale;
    oy = ((float)height - span_y * scale) * 0.5f + max_y * scale;
    SDL_SetRenderDrawColor(renderer, 12, 16, 24, 255); SDL_RenderClear(renderer);
    for (uint32_t i = 0u; i < frame->line_wall_count; ++i) {
        const DatalabLineWall *wall = &frame->line_walls[i];
        const DatalabLineAnchor *a = &frame->line_anchors[wall->anchor_a];
        const DatalabLineAnchor *b = &frame->line_anchors[wall->anchor_b];
        SDL_SetRenderDrawColor(renderer, wall->lock_length ? 241 : 104, wall->lock_length ? 196 : 205, wall->lock_length ? 15 : 222, 255);
        SDL_RenderDrawLine(renderer, (int)(ox + a->x * scale), (int)(oy - a->y * scale), (int)(ox + b->x * scale), (int)(oy - b->y * scale));
    }
    SDL_SetRenderDrawColor(renderer, 68, 224, 255, 255);
    for (uint32_t i = 0u; i < frame->line_anchor_count; ++i) {
        SDL_Rect marker = { (int)(ox + frame->line_anchors[i].x * scale) - 3, (int)(oy - frame->line_anchors[i].y * scale) - 3, 7, 7 };
        SDL_RenderFillRect(renderer, &marker);
    }
    SDL_RenderPresent(renderer); outcome->result = core_result_ok(); outcome->presented = 1u;
    return outcome->result;
}

CoreResult datalab_line_diagnostic_submit_frame(SDL_Window *window, SDL_Renderer *renderer, const DatalabFrame *frame, DatalabRenderSubmitOutcome *outcome) {
    (void)window;
    return datalab_line_diagnostic_draw(renderer, frame, outcome);
}

static CoreResult datalab_loop_render_step_line_diagnostic(SDL_Window *window, SDL_Renderer *renderer, const DatalabFrame *frame, DatalabAppState *app_state, void *lane_ctx, DatalabRenderSubmitOutcome *outcome) {
    (void)app_state; (void)lane_ctx;
    return datalab_line_diagnostic_submit_frame(window, renderer, frame, outcome);
}

CoreResult render_line_diagnostic_loop(SDL_Window *window, SDL_Renderer *renderer, const DatalabFrame *frame, DatalabAppState *app_state) {
    const DatalabLoopProfileOps ops = { .lane_tag = "line_diagnostic", .lane_ctx = NULL, .render_step = datalab_loop_render_step_line_diagnostic };
    return datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
}
