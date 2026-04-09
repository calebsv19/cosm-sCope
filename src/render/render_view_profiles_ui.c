#include "render/render_view_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kit_graph_timeseries.h"

typedef struct TraceLaneMeta {
    char lane[32];
    float min_v;
    float max_v;
    size_t count;
    float current_value;
    double current_dt;
    int has_current_value;
} TraceLaneMeta;

typedef struct TraceGridSpec {
    int time_divisions;
    int value_divisions;
    double time_unit_scale;
    float value_unit_scale;
} TraceGridSpec;

static const float TRACE_FLATLINE_BASELINE = 0.12f;

static int cmp_double_asc(const void *a, const void *b) {
    const double da = *(const double *)a;
    const double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static SDL_Color trace_lane_color(size_t lane_idx) {
    static const SDL_Color kPalette[] = {
        {185, 245, 220, 255},
        {255, 205, 120, 255},
        {145, 205, 255, 255},
        {220, 180, 255, 255},
        {255, 150, 150, 255},
        {160, 235, 170, 255}
    };
    return kPalette[lane_idx % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

static void trace_compact_label(const char *label, char *out, size_t out_cap) {
    size_t n = 0u;
    if (!out || out_cap == 0u) {
        return;
    }
    if (!label) {
        out[0] = '\0';
        return;
    }
    n = strlen(label);
    if (n < 20u) {
        snprintf(out, out_cap, "%s", label);
        return;
    }
    snprintf(out, out_cap, "%.16s...", label);
}

static void draw_trace_time_grid(SDL_Renderer *renderer,
                                 const SDL_Rect *plot,
                                 double min_t,
                                 double max_t,
                                 int label_y,
                                 const TraceGridSpec *grid) {
    if (!renderer || !plot || !grid || plot->w < 4 || plot->h < 4) return;
    if (grid->time_divisions <= 0) return;

    for (int i = 0; i <= grid->time_divisions; ++i) {
        double ratio = (double)i / (double)grid->time_divisions;
        int x = plot->x + (int)(ratio * (double)(plot->w - 1));
        double t = min_t + ratio * (max_t - min_t);
        double t_scaled = (grid->time_unit_scale > 0.0) ? (t / grid->time_unit_scale) : t;
        char label[32];

        SDL_SetRenderDrawColor(renderer, 42, 46, 58, (i == 0 || i == grid->time_divisions) ? 120 : 90);
        SDL_RenderDrawLine(renderer, x, plot->y, x, plot->y + plot->h - 1);

        snprintf(label, sizeof(label), "%.2f", t_scaled);
        draw_text_5x7(renderer, x - datalab_scaled_px(14.0f), label_y, label, 1, 120, 130, 150, 255);
    }
}

static void draw_trace_value_grid(SDL_Renderer *renderer,
                                  const SDL_Rect *band,
                                  float min_v,
                                  float max_v,
                                  const TraceGridSpec *grid) {
    if (!renderer || !band || !grid || band->w < 4 || band->h < 4) return;
    if (grid->value_divisions <= 0) return;

    for (int i = 0; i <= grid->value_divisions; ++i) {
        double ratio = (double)i / (double)grid->value_divisions;
        int y = band->y + band->h - 1 - (int)(ratio * (double)(band->h - 1));
        float v = min_v + (float)ratio * (max_v - min_v);
        float v_scaled = (grid->value_unit_scale > 0.0f) ? (v / grid->value_unit_scale) : v;
        char label[24];

        SDL_SetRenderDrawColor(renderer, 50, 55, 68, (i == 0 || i == grid->value_divisions) ? 110 : 85);
        SDL_RenderDrawLine(renderer, band->x, y, band->x + band->w - 1, y);

        if (i == 0 || i == grid->value_divisions || i == grid->value_divisions / 2) {
            int label_y;
            snprintf(label, sizeof(label), "%.2f", v_scaled);
            label_y = clamp_int(y - datalab_scaled_px(4.0f),
                                band->y + 1,
                                band->y + band->h - datalab_scaled_px(8.0f));
            draw_text_5x7(renderer,
                          band->x + band->w - datalab_scaled_px(34.0f),
                          label_y,
                          label,
                          1,
                          120,
                          130,
                          150,
                          255);
        }
    }
}

static void draw_trace_legend(SDL_Renderer *renderer, const TraceLaneMeta *lanes, size_t lane_count, const SDL_Rect *header_rect) {
    int pad = 0;
    int row_gap = 0;
    int swatch_w = 0;
    int swatch_h = 0;
    int title_h = 0;
    int row_h = 0;
    int max_label_w = 0;
    int title_w = 0;
    int panel_w = 0;
    int panel_h = 0;
    int content_h = 0;
    SDL_Rect panel;
    if (!renderer || !lanes || !header_rect) return;
    (void)datalab_measure_text(1, "TRACE LANES", &title_w, &title_h);
    row_h = datalab_text_line_height(1);
    pad = datalab_scaled_px(8.0f);
    row_gap = datalab_scaled_px(3.0f);
    swatch_w = datalab_scaled_px(10.0f);
    swatch_h = datalab_scaled_px(6.0f);
    for (size_t i = 0; i < lane_count; ++i) {
        int w = 0;
        int h = 0;
        (void)datalab_measure_text(1, lanes[i].lane, &w, &h);
        if (w > max_label_w) {
            max_label_w = w;
        }
    }
    panel_w = pad + swatch_w + datalab_scaled_px(6.0f) + max_label_w + pad;
    if (title_w + (pad * 2) > panel_w) {
        panel_w = title_w + (pad * 2);
    }
    panel_w = clamp_int(panel_w, datalab_scaled_px(150.0f), header_rect->w / 2);

    content_h = (int)lane_count * row_h;
    if (lane_count > 1u) {
        content_h += ((int)lane_count - 1) * row_gap;
    }
    panel_h = pad + title_h + row_gap + content_h + pad;
    panel_h = clamp_int(panel_h, datalab_scaled_px(42.0f), header_rect->h - datalab_scaled_px(6.0f));

    panel = (SDL_Rect){
        header_rect->x + datalab_scaled_px(8.0f),
        header_rect->y + (header_rect->h - panel_h) / 2,
        panel_w,
        panel_h
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 182);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 220);
    SDL_RenderDrawRect(renderer, &panel);

    draw_text_5x7(renderer,
                  panel.x + pad,
                  panel.y + pad,
                  "TRACE LANES",
                  1,
                  215,
                  220,
                  235,
                  255);
    for (size_t i = 0; i < lane_count; ++i) {
        SDL_Color c = trace_lane_color(i);
        int y = panel.y + pad + title_h + row_gap + (int)i * (row_h + row_gap);
        if (y > panel.y + panel.h - row_h) break;
        SDL_Rect swatch = {
            panel.x + pad,
            y + (row_h - swatch_h) / 2,
            swatch_w,
            swatch_h
        };
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(renderer, &swatch);
        draw_text_5x7(renderer,
                      panel.x + pad + swatch_w + datalab_scaled_px(6.0f),
                      y,
                      lanes[i].lane,
                      1,
                      205,
                      210,
                      220,
                      255);
    }
}

static void draw_trace_readout(SDL_Renderer *renderer,
                               const TraceLaneMeta *lanes,
                               size_t lane_count,
                               double play_t,
                               size_t total_samples,
                               size_t total_markers,
                               double min_t,
                               double max_t,
                               const char *nearest_marker_label,
                               double nearest_marker_t,
                               const SDL_Rect *header_rect) {
    if (!renderer || !lanes || !header_rect) return;

    {
        int pad = datalab_scaled_px(8.0f);
        int row_gap = datalab_scaled_px(2.0f);
        int row_h = datalab_text_line_height(1);
        int swatch_w = datalab_scaled_px(9.0f);
        int swatch_h = datalab_scaled_px(6.0f);
        int max_text_w = 0;
        int panel_w = 0;
        int panel_h = 0;
        int cursor_y = 0;
        char line[64];
        char summary[96];
        char status_line[128];
        char marker_line[112];
        char compact_marker[40];
        SDL_Rect panel;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        snprintf(line, sizeof(line), "T %.4f", play_t);
        snprintf(summary,
                 sizeof(summary),
                 "S:%zu M:%zu DUR:%.4f",
                 total_samples,
                 total_markers,
                 (max_t > min_t) ? (max_t - min_t) : 0.0);
        snprintf(status_line, sizeof(status_line), "%s | %s", line, summary);
        trace_compact_label(nearest_marker_label, compact_marker, sizeof(compact_marker));
        if (nearest_marker_label && nearest_marker_label[0] != '\0') {
            snprintf(marker_line, sizeof(marker_line), "MKR %.4f %s", nearest_marker_t, compact_marker);
        } else {
            snprintf(marker_line, sizeof(marker_line), "MKR N/A");
        }

        (void)datalab_measure_text(1, status_line, &max_text_w, &panel_h);
        {
            int measure_w = 0;
            int measure_h = 0;
            (void)datalab_measure_text(1, marker_line, &measure_w, &measure_h);
            if (measure_w > max_text_w) max_text_w = measure_w;
        }
        for (size_t i = 0; i < lane_count; ++i) {
            char value_line[96];
            int measure_w = 0;
            int measure_h = 0;
            if (lanes[i].has_current_value) {
                snprintf(value_line, sizeof(value_line), "%s: %.4f", lanes[i].lane, lanes[i].current_value);
            } else {
                snprintf(value_line, sizeof(value_line), "%s: N/A", lanes[i].lane);
            }
            (void)datalab_measure_text(1, value_line, &measure_w, &measure_h);
            measure_w += swatch_w + datalab_scaled_px(8.0f);
            if (measure_w > max_text_w) {
                max_text_w = measure_w;
            }
        }

        panel_w = max_text_w + (pad * 2);
        panel_w = clamp_int(panel_w, datalab_scaled_px(180.0f), (header_rect->w * 40) / 100);
        panel_h = pad + row_h + row_gap + row_h + row_gap;
        panel_h += (int)lane_count * (row_h + row_gap);
        panel_h += pad;
        panel_h = clamp_int(panel_h, datalab_scaled_px(42.0f), header_rect->h - datalab_scaled_px(6.0f));
        panel = (SDL_Rect){
            header_rect->x + header_rect->w - panel_w - datalab_scaled_px(8.0f),
            header_rect->y + (header_rect->h - panel_h) / 2,
            panel_w,
            panel_h
        };
        SDL_SetRenderDrawColor(renderer, 8, 10, 16, 182);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 95, 110, 220);
        SDL_RenderDrawRect(renderer, &panel);

        cursor_y = panel.y + pad;
        draw_text_5x7(renderer,
                      panel.x + pad,
                      cursor_y,
                      status_line,
                      1,
                      230,
                      230,
                      235,
                      255);
        cursor_y += row_h + row_gap;
        draw_text_5x7(renderer,
                      panel.x + pad,
                      cursor_y,
                      marker_line,
                      1,
                      250,
                      180,
                      120,
                      255);

        cursor_y += row_h + row_gap;
        for (size_t i = 0; i < lane_count; ++i) {
            SDL_Color c = trace_lane_color(i);
            int y = cursor_y + (int)i * (row_h + row_gap);
            if (y > panel.y + panel.h - row_h) break;
            SDL_Rect swatch = {
                panel.x + pad,
                y + (row_h - swatch_h) / 2,
                swatch_w,
                swatch_h
            };
            char value_line[96];
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(renderer, &swatch);
            if (lanes[i].has_current_value) {
                snprintf(value_line, sizeof(value_line), "%s: %.4f", lanes[i].lane, lanes[i].current_value);
            } else {
                snprintf(value_line, sizeof(value_line), "%s: N/A", lanes[i].lane);
            }
            draw_text_5x7(renderer,
                          panel.x + pad + swatch_w + datalab_scaled_px(6.0f),
                          y,
                          value_line,
                          1,
                          215,
                          220,
                          235,
                          255);
        }
    }
}

static void render_trace_frame(SDL_Renderer *renderer, const DatalabFrame *frame, DatalabAppState *app_state) {
    TraceLaneMeta lanes[12];
    size_t lane_count = 0;
    double *time_points = NULL;
    size_t time_count = 0;
    int ww = 0;
    int wh = 0;
    SDL_Rect header = {0};
    SDL_Rect plot = {0};
    double min_t = 0.0;
    double max_t = 0.0;
    double play_t = 0.0;
    int trace_time_label_y = 0;
    int footer_y = 0;
    TraceGridSpec grid = {8, 4, 1.0, 1.0f};

    if (!renderer || !frame || !app_state || !frame->trace_samples || frame->trace_sample_count == 0) {
        return;
    }
    datalab_sync_text_zoom(app_state);

    memset(lanes, 0, sizeof(lanes));

    min_t = frame->trace_samples[0].time_seconds;
    max_t = frame->trace_samples[0].time_seconds;
    for (size_t i = 0; i < frame->trace_sample_count; ++i) {
        const DatalabTraceSample *s = &frame->trace_samples[i];
        size_t lane_idx = (size_t)-1;
        for (size_t l = 0; l < lane_count; ++l) {
            if (lane_name_eq(lanes[l].lane, s->lane)) {
                lane_idx = l;
                break;
            }
        }
        if (lane_idx == (size_t)-1 && lane_count < (sizeof(lanes) / sizeof(lanes[0]))) {
            lane_idx = lane_count++;
            snprintf(lanes[lane_idx].lane, sizeof(lanes[lane_idx].lane), "%s", s->lane);
            lanes[lane_idx].min_v = s->value;
            lanes[lane_idx].max_v = s->value;
        }
        if (lane_idx != (size_t)-1) {
            if (s->value < lanes[lane_idx].min_v) lanes[lane_idx].min_v = s->value;
            if (s->value > lanes[lane_idx].max_v) lanes[lane_idx].max_v = s->value;
            lanes[lane_idx].count++;
        }
        if (s->time_seconds < min_t) min_t = s->time_seconds;
        if (s->time_seconds > max_t) max_t = s->time_seconds;
    }

    time_points = (double *)core_alloc(frame->trace_sample_count * sizeof(double));
    if (!time_points) return;
    for (size_t i = 0; i < frame->trace_sample_count; ++i) {
        time_points[time_count++] = frame->trace_samples[i].time_seconds;
    }
    qsort(time_points, time_count, sizeof(double), cmp_double_asc);
    {
        size_t write_idx = 0u;
        for (size_t read_idx = 0u; read_idx < time_count; ++read_idx) {
            double t = time_points[read_idx];
            if (write_idx == 0u || fabs(t - time_points[write_idx - 1u]) > 1e-9) {
                time_points[write_idx++] = t;
            }
        }
        time_count = write_idx;
    }

    if (time_count == 0) {
        core_free(time_points);
        return;
    }
    if (app_state->trace_cursor_index == (size_t)-1) {
        app_state->trace_cursor_index = time_count - 1u;
    } else if (app_state->trace_cursor_index >= time_count) {
        app_state->trace_cursor_index = time_count - 1u;
    }
    play_t = time_points[app_state->trace_cursor_index];

    for (size_t l = 0; l < lane_count; ++l) {
        lanes[l].has_current_value = 0;
        lanes[l].current_dt = 1e30;
        lanes[l].current_value = 0.0f;
    }
    for (size_t i = 0; i < frame->trace_sample_count; ++i) {
        const DatalabTraceSample *s = &frame->trace_samples[i];
        for (size_t l = 0; l < lane_count; ++l) {
            if (!lane_name_eq(s->lane, lanes[l].lane)) continue;
            {
                double dt = fabs(s->time_seconds - play_t);
                if (!lanes[l].has_current_value || dt < lanes[l].current_dt) {
                    lanes[l].current_dt = dt;
                    lanes[l].current_value = s->value;
                    lanes[l].has_current_value = 1;
                }
            }
            break;
        }
    }

    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    header.x = datalab_scaled_px(32.0f);
    header.y = datalab_scaled_px(8.0f);
    header.w = ww - datalab_scaled_px(64.0f);
    header.h = datalab_scaled_px(64.0f);
    plot.x = datalab_scaled_px(32.0f);
    plot.y = header.y + header.h + datalab_scaled_px(6.0f);
    plot.w = ww - datalab_scaled_px(64.0f);
    plot.h = wh - plot.y - datalab_scaled_px(42.0f);
    if (plot.w < 40 || plot.h < 40) {
        core_free(time_points);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 10, 10, 14, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 20, 23, 31, 255);
    SDL_RenderFillRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 80, 86, 104, 255);
    SDL_RenderDrawRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 24, 27, 36, 255);
    SDL_RenderFillRect(renderer, &plot);
    trace_time_label_y = plot.y + plot.h + 6;
    draw_trace_time_grid(renderer, &plot, min_t, max_t, trace_time_label_y, &grid);

    if (lane_count == 0) lane_count = 1;
    if (app_state->trace_lane_cycle_requested) {
        size_t cycle_span = lane_count + 1u; /* all + per-lane */
        app_state->trace_lane_visibility_index = (app_state->trace_lane_visibility_index + 1u) % cycle_span;
        app_state->trace_lane_cycle_requested = 0;
    } else if (app_state->trace_lane_visibility_index > lane_count) {
        app_state->trace_lane_visibility_index = 0u;
    }

    {
        size_t visible_lane_count = (app_state->trace_lane_visibility_index == 0u) ? lane_count : 1u;
        size_t visible_lane_cursor = 0u;
        for (size_t l = 0; l < lane_count; ++l) {
        if (app_state->trace_lane_visibility_index > 0u &&
            app_state->trace_lane_visibility_index != (l + 1u)) {
            continue;
        }
            int by = plot.y + (int)((double)visible_lane_cursor * (double)plot.h / (double)visible_lane_count);
            int bh = (int)((double)plot.h / (double)visible_lane_count);
        SDL_Rect band = {
            plot.x + datalab_scaled_px(8.0f),
            by + datalab_scaled_px(4.0f),
            plot.w - datalab_scaled_px(16.0f),
            bh - datalab_scaled_px(8.0f)
        };
        if (band.h < 12) continue;

        SDL_SetRenderDrawColor(renderer, 31, 35, 46, 255);
        SDL_RenderFillRect(renderer, &band);
        SDL_SetRenderDrawColor(renderer, 64, 70, 86, 255);
        SDL_RenderDrawRect(renderer, &band);
        draw_trace_value_grid(renderer, &band, lanes[l].min_v, lanes[l].max_v, &grid);

        draw_text_5x7(renderer,
                      band.x + datalab_scaled_px(6.0f),
                      band.y + datalab_scaled_px(6.0f),
                      lanes[l].lane,
                      1,
                      170,
                      220,
                      240,
                      255);

        float span = lanes[l].max_v - lanes[l].min_v;
        int is_flat_lane = fabsf(span) < 1e-8f;
        uint32_t lane_stride = kit_graph_ts_recommended_stride((uint32_t)lanes[l].count, (float)band.w, 0u);
        size_t lane_sample_index = 0u;
        size_t last_lane_sample_index = lanes[l].count > 0 ? lanes[l].count - 1u : 0u;
        if (is_flat_lane) span = 1.0f;
        int have_prev = 0;
        int prev_x = 0;
        int prev_y = 0;
        {
            SDL_Color lane_color = trace_lane_color(l);
            SDL_SetRenderDrawColor(renderer, lane_color.r, lane_color.g, lane_color.b, 255);
        }
        for (size_t i = 0; i < frame->trace_sample_count; ++i) {
            const DatalabTraceSample *s = &frame->trace_samples[i];
            if (!lane_name_eq(s->lane, lanes[l].lane)) continue;
            {
                int should_draw = ((lane_sample_index % lane_stride) == 0u) ||
                                  (lane_sample_index == last_lane_sample_index);
                lane_sample_index++;
                if (!should_draw) {
                    continue;
                }
            }
            double tr = (max_t > min_t) ? ((s->time_seconds - min_t) / (max_t - min_t)) : 0.0;
            float vr = is_flat_lane ? TRACE_FLATLINE_BASELINE : ((s->value - lanes[l].min_v) / span);
            if (vr < 0.0f) vr = 0.0f;
            if (vr > 1.0f) vr = 1.0f;
            int x = band.x + (int)(tr * (double)(band.w - 1));
            int y = band.y + band.h - 1 - (int)(vr * (float)(band.h - 1));
            if (have_prev) {
                SDL_RenderDrawLine(renderer, prev_x, prev_y, x, y);
            }
            prev_x = x;
            prev_y = y;
            have_prev = 1;
        }
            visible_lane_cursor++;
        }
    }

    if (frame->trace_marker_count > 0) {
        for (size_t i = 0; i < frame->trace_marker_count; ++i) {
            const DatalabTraceMarker *m = &frame->trace_markers[i];
            double tr = (max_t > min_t) ? ((m->time_seconds - min_t) / (max_t - min_t)) : 0.0;
            int x = plot.x + (int)(tr * (double)(plot.w - 1));
        SDL_SetRenderDrawColor(renderer, 250, 170, 95, 220);
        SDL_RenderDrawLine(renderer, x, plot.y, x, plot.y + plot.h - 1);
    }
    }

    {
        double pr = (max_t > min_t) ? ((play_t - min_t) / (max_t - min_t)) : 0.0;
        int x = plot.x + (int)(pr * (double)(plot.w - 1));
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, x, plot.y, x, plot.y + plot.h - 1);
    }

    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 255);
    SDL_RenderDrawRect(renderer, &plot);
    {
        const DatalabTraceMarker *nearest_marker = NULL;
        double nearest_marker_dt = 1e30;
        for (size_t i = 0; i < frame->trace_marker_count; ++i) {
            const DatalabTraceMarker *m = &frame->trace_markers[i];
            double dt = fabs(m->time_seconds - play_t);
            if (dt < nearest_marker_dt) {
                nearest_marker_dt = dt;
                nearest_marker = m;
            }
        }
        draw_trace_legend(renderer, lanes, lane_count, &header);
        draw_trace_readout(renderer,
                           lanes,
                           lane_count,
                           play_t,
                           frame->trace_sample_count,
                           frame->trace_marker_count,
                           min_t,
                           max_t,
                           nearest_marker ? nearest_marker->label : "",
                           nearest_marker ? nearest_marker->time_seconds : 0.0,
                           &header);
    }
    draw_text_5x7(renderer,
                  plot.x + datalab_scaled_px(10.0f),
                  trace_time_label_y + datalab_scaled_px(10.0f),
                  "TIME SECONDS",
                  1,
                  120,
                  130,
                  150,
                  255);
    draw_text_5x7(renderer,
                  plot.x + datalab_scaled_px(120.0f),
                  trace_time_label_y + datalab_scaled_px(10.0f),
                  "GRID T=SECONDS V=VALUE",
                  1,
                  120,
                  130,
                  150,
                  255);

    footer_y = wh - datalab_scaled_px(14.0f);
    draw_text_5x7(renderer,
                  plot.x + datalab_scaled_px(10.0f),
                  footer_y,
                  "LEFT RIGHT SCRUB",
                  1,
                  205,
                  210,
                  220,
                  255);
    draw_text_5x7(renderer,
                  plot.x + datalab_scaled_px(180.0f),
                  footer_y,
                  "Z ZOOM-STUB",
                  1,
                  160,
                  175,
                  190,
                  255);
    draw_text_5x7(renderer,
                  plot.x + datalab_scaled_px(286.0f),
                  footer_y,
                  "X STATS-STUB",
                  1,
                  160,
                  175,
                  190,
                  255);
    draw_text_5x7(renderer,
                  plot.x + datalab_scaled_px(392.0f),
                  footer_y,
                  "C LANE-CYCLE",
                  1,
                  160,
                  175,
                  190,
                  255);

    core_free(time_points);
}

void datalab_render_derive_frame(const DatalabFrame *frame,
                                 const DatalabAppState *app_state,
                                 DatalabRenderDeriveFrame *out_derive) {
    if (!frame || !app_state || !out_derive) {
        return;
    }
    memset(out_derive, 0, sizeof(*out_derive));
    make_title(frame, app_state, out_derive->title, sizeof(out_derive->title));
}

void datalab_physics_render_derive_frame(SDL_Renderer *renderer,
                                         const DatalabFrame *frame,
                                         const DatalabAppState *app_state,
                                         const uint8_t *density_rgba,
                                         const uint8_t *speed_rgba,
                                         DatalabPhysicsRenderDeriveFrame *out_derive) {
    int ww = 0;
    int wh = 0;
    if (!renderer || !frame || !app_state || !density_rgba || !speed_rgba || !out_derive) {
        return;
    }
    memset(out_derive, 0, sizeof(*out_derive));
    datalab_render_derive_frame(frame, app_state, &out_derive->common);
    out_derive->pixels = density_rgba;
    if (app_state->view_mode == DATALAB_VIEW_SPEED) {
        out_derive->pixels = speed_rgba;
    }
    out_derive->draw_vectors = (uint8_t)(app_state->view_mode == DATALAB_VIEW_DENSITY_VECTOR);
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    calc_fit_rect(ww, wh, frame->width, frame->height, &out_derive->dst);
}

void datalab_physics_render_submit_frame(SDL_Window *window,
                                         SDL_Renderer *renderer,
                                         SDL_Texture *texture,
                                         const DatalabFrame *frame,
                                         const DatalabAppState *app_state,
                                         KitVizVecSegment *segments,
                                         size_t sample_count,
                                         const DatalabPhysicsRenderDeriveFrame *derive,
                                         DatalabRenderSubmitOutcome *outcome) {
    if (!outcome) {
        return;
    }
    memset(outcome, 0, sizeof(*outcome));
    if (!window || !renderer || !texture || !frame || !app_state || !segments || !derive || !derive->pixels) {
        outcome->result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid physics render submit request" };
        return;
    }

    datalab_sync_text_zoom(app_state);
    SDL_UpdateTexture(texture, NULL, derive->pixels, (int)frame->width * 4);
    SDL_SetRenderDrawColor(renderer, 10, 10, 14, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &derive->dst);

    if (derive->draw_vectors) {
        size_t seg_count = 0;
        CoreResult rv = kit_viz_build_vector_segments(frame->velx,
                                                      frame->vely,
                                                      frame->width,
                                                      frame->height,
                                                      app_state->vector_stride,
                                                      app_state->vector_scale,
                                                      segments,
                                                      sample_count,
                                                      &seg_count);
        if (rv.code == CORE_OK) {
            size_t i = 0u;
            SDL_SetRenderDrawColor(renderer, 230, 255, 230, 255);
            for (i = 0u; i < seg_count; ++i) {
                const float x0 = (float)derive->dst.x + (segments[i].x0 / (float)frame->width) * (float)derive->dst.w;
                const float y0 = (float)derive->dst.y + (segments[i].y0 / (float)frame->height) * (float)derive->dst.h;
                const float x1 = (float)derive->dst.x + (segments[i].x1 / (float)frame->width) * (float)derive->dst.w;
                const float y1 = (float)derive->dst.y + (segments[i].y1 / (float)frame->height) * (float)derive->dst.h;
                SDL_RenderDrawLine(renderer, (int)x0, (int)y0, (int)x1, (int)y1);
            }
        }
    }

    datalab_draw_session_controls(renderer, app_state);
    SDL_SetWindowTitle(window, derive->common.title);
    SDL_RenderPresent(renderer);
    outcome->presented = 1u;
    outcome->result = core_result_ok();
}

void datalab_daw_render_submit_frame(SDL_Window *window,
                                     SDL_Renderer *renderer,
                                     const DatalabFrame *frame,
                                     const DatalabAppState *app_state,
                                     const DatalabRenderDeriveFrame *derive,
                                     DatalabRenderSubmitOutcome *outcome) {
    if (!outcome) {
        return;
    }
    memset(outcome, 0, sizeof(*outcome));
    if (!window || !renderer || !frame || !app_state || !derive) {
        outcome->result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid daw render submit request" };
        return;
    }
    render_daw_frame(renderer, frame, app_state);
    datalab_draw_session_controls(renderer, app_state);
    SDL_SetWindowTitle(window, derive->title);
    SDL_RenderPresent(renderer);
    outcome->presented = 1u;
    outcome->result = core_result_ok();
}

void datalab_trace_render_submit_frame(SDL_Window *window,
                                       SDL_Renderer *renderer,
                                       const DatalabFrame *frame,
                                       DatalabAppState *app_state,
                                       const DatalabRenderDeriveFrame *derive,
                                       DatalabRenderSubmitOutcome *outcome) {
    if (!outcome) {
        return;
    }
    memset(outcome, 0, sizeof(*outcome));
    if (!window || !renderer || !frame || !app_state || !derive) {
        outcome->result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid trace render submit request" };
        return;
    }
    render_trace_frame(renderer, frame, app_state);
    datalab_draw_session_controls(renderer, app_state);
    SDL_SetWindowTitle(window, derive->title);
    SDL_RenderPresent(renderer);
    outcome->presented = 1u;
    outcome->result = core_result_ok();
}

void datalab_rs1_diag_note(const char *lane,
                           DatalabRenderDiagTotals *totals,
                           const DatalabRenderSubmitOutcome *submit) {
    if (!totals || !submit) {
        return;
    }
    totals->frame_count += 1u;
    if (submit->result.code == CORE_OK) {
        totals->submit_count += 1u;
    }
    if (submit->presented) {
        totals->present_count += 1u;
    }
    if (datalab_rs1_diag_enabled()) {
        printf("[rs1] datalab-%s frame=%llu submit_ok=%d presented=%u totals(frames=%llu submit_ok=%llu present=%llu)\n",
               lane ? lane : "unknown",
               (unsigned long long)totals->frame_count,
               submit->result.code == CORE_OK ? 1 : 0,
               (unsigned int)submit->presented,
               (unsigned long long)totals->frame_count,
               (unsigned long long)totals->submit_count,
               (unsigned long long)totals->present_count);
    }
}
