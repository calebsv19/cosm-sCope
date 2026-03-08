#include "render/render_view.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "kit_graph_timeseries.h"
#include "kit_viz.h"
#include "ui/input.h"

static const char *daw_view_mode_name(DatalabViewMode mode) {
    switch (mode) {
        case DATALAB_VIEW_DENSITY: return "markers";
        case DATALAB_VIEW_SPEED: return "waveform";
        case DATALAB_VIEW_DENSITY_VECTOR: return "waveform+markers";
        default: return "unknown";
    }
}

static int lane_name_eq(const char *a, const char *b) {
    return strncmp(a, b, 31) == 0;
}

static void make_title(const DatalabFrame *frame, const DatalabAppState *state, char *title, size_t title_cap) {
    if (!frame || !state || !title || title_cap == 0) return;

    if (frame->profile == DATALAB_PROFILE_DAW) {
        snprintf(title,
                 title_cap,
                 "DataLab | DAW points=%llu markers=%zu sr=%u mode=%s",
                 (unsigned long long)frame->point_count,
                 frame->marker_count,
                 frame->sample_rate,
                 daw_view_mode_name(state->view_mode));
        return;
    }

    if (frame->profile == DATALAB_PROFILE_TRACE) {
        size_t cursor = state->trace_cursor_index;
        if (frame->trace_sample_count == 0) cursor = 0;
        snprintf(title,
                 title_cap,
                 "DataLab | TRACE samples=%zu markers=%zu cursor=%zu zoom_stub=%.2f stats_stub=%s",
                 frame->trace_sample_count,
                 frame->trace_marker_count,
                 cursor,
                 state->trace_zoom_stub,
                 state->trace_selection_stub_active ? "on" : "off");
        return;
    }

    snprintf(title,
             title_cap,
             "DataLab | frame=%llu grid=%ux%u t=%.3f dt=%.3f mode=%s stride=%u",
             (unsigned long long)frame->frame_index,
             frame->width,
             frame->height,
             frame->time_seconds,
             frame->dt_seconds,
             datalab_view_mode_name(state->view_mode),
             state->vector_stride);
}

static void calc_fit_rect(int ww, int wh, uint32_t fw, uint32_t fh, SDL_Rect *out_rect) {
    if (!out_rect || fw == 0 || fh == 0 || ww <= 0 || wh <= 0) return;

    const float sx = (float)ww / (float)fw;
    const float sy = (float)wh / (float)fh;
    const float scale = sx < sy ? sx : sy;

    const int rw = (int)((float)fw * scale);
    const int rh = (int)((float)fh * scale);
    out_rect->x = (ww - rw) / 2;
    out_rect->y = (wh - rh) / 2;
    out_rect->w = rw;
    out_rect->h = rh;
}

static float clamp_unit(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static const char *glyph5x7(char c) {
    switch (c) {
        case 'A': return "01110""10001""10001""11111""10001""10001""10001";
        case 'D': return "11110""10001""10001""10001""10001""10001""11110";
        case 'B': return "11110""10001""10001""11110""10001""10001""11110";
        case 'C': return "01111""10000""10000""10000""10000""10000""01111";
        case 'E': return "11111""10000""10000""11110""10000""10000""11111";
        case 'F': return "11111""10000""10000""11110""10000""10000""10000";
        case 'G': return "01110""10001""10000""10011""10001""10001""01110";
        case 'H': return "10001""10001""10001""11111""10001""10001""10001";
        case 'I': return "11111""00100""00100""00100""00100""00100""11111";
        case 'K': return "10001""10010""10100""11000""10100""10010""10001";
        case 'L': return "10000""10000""10000""10000""10000""10000""11111";
        case 'M': return "10001""11011""10101""10101""10001""10001""10001";
        case 'N': return "10001""11001""10101""10011""10001""10001""10001";
        case 'O': return "01110""10001""10001""10001""10001""10001""01110";
        case 'P': return "11110""10001""10001""11110""10000""10000""10000";
        case 'R': return "11110""10001""10001""11110""10100""10010""10001";
        case 'S': return "01111""10000""10000""01110""00001""00001""11110";
        case 'T': return "11111""00100""00100""00100""00100""00100""00100";
        case 'U': return "10001""10001""10001""10001""10001""10001""01110";
        case 'V': return "10001""10001""10001""10001""10001""01010""00100";
        case 'W': return "10001""10001""10001""10101""10101""10101""01010";
        case 'X': return "10001""10001""01010""00100""01010""10001""10001";
        case 'Y': return "10001""10001""01010""00100""00100""00100""00100";
        case 'Z': return "11111""00001""00010""00100""01000""10000""11111";
        case '0': return "01110""10001""10011""10101""11001""10001""01110";
        case '1': return "00100""01100""00100""00100""00100""00100""01110";
        case '2': return "01110""10001""00001""00010""00100""01000""11111";
        case '3': return "11110""00001""00001""01110""00001""00001""11110";
        case '4': return "00010""00110""01010""10010""11111""00010""00010";
        case '5': return "11111""10000""11110""00001""00001""10001""01110";
        case '6': return "00110""01000""10000""11110""10001""10001""01110";
        case '7': return "11111""00001""00010""00100""01000""01000""01000";
        case '8': return "01110""10001""10001""01110""10001""10001""01110";
        case '9': return "01110""10001""10001""01111""00001""00010""11100";
        case '.': return "00000""00000""00000""00000""00000""00110""00110";
        case '-': return "00000""00000""00000""11111""00000""00000""00000";
        case '_': return "00000""00000""00000""00000""00000""00000""11111";
        case '+': return "00000""00100""00100""11111""00100""00100""00000";
        case ':': return "00000""00100""00100""00000""00100""00100""00000";
        case '=': return "00000""11111""00000""11111""00000""00000""00000";
        case 'a': return glyph5x7('A');
        case 'b': return glyph5x7('B');
        case 'c': return glyph5x7('C');
        case 'd': return glyph5x7('D');
        case 'e': return glyph5x7('E');
        case 'f': return glyph5x7('F');
        case 'g': return glyph5x7('G');
        case 'h': return glyph5x7('H');
        case 'i': return glyph5x7('I');
        case 'j': return glyph5x7('I');
        case 'k': return glyph5x7('K');
        case 'l': return glyph5x7('L');
        case 'm': return glyph5x7('M');
        case 'n': return glyph5x7('N');
        case 'o': return glyph5x7('O');
        case 'p': return glyph5x7('P');
        case 'q': return glyph5x7('O');
        case 'r': return glyph5x7('R');
        case 's': return glyph5x7('S');
        case 't': return glyph5x7('T');
        case 'u': return glyph5x7('U');
        case 'v': return glyph5x7('V');
        case 'w': return glyph5x7('W');
        case 'x': return glyph5x7('X');
        case 'y': return glyph5x7('Y');
        case 'z': return glyph5x7('Z');
        case ' ': return "00000""00000""00000""00000""00000""00000""00000";
        default: return "00000""00000""00000""00000""00000""00000""00000";
    }
}

static void draw_text_5x7(SDL_Renderer *renderer,
                          int x,
                          int y,
                          const char *text,
                          int scale,
                          uint8_t r,
                          uint8_t g,
                          uint8_t b,
                          uint8_t a) {
    if (!renderer || !text || scale < 1) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    for (int i = 0; text[i] != '\0'; ++i) {
        const char *bits = glyph5x7(text[i]);
        int gx = x + i * (6 * scale);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                char bit = bits[row * 5 + col];
                if (bit == '1') {
                    SDL_Rect px = { gx + col * scale, y + row * scale, scale, scale };
                    SDL_RenderFillRect(renderer, &px);
                }
            }
        }
    }
}

static void draw_daw_legend(SDL_Renderer *renderer, const DatalabAppState *app_state, const SDL_Rect *plot_rect) {
    if (!renderer || !app_state || !plot_rect) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect panel = { plot_rect->x + 8, plot_rect->y + 8, 258, 64 };
    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 180);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 210);
    SDL_RenderDrawRect(renderer, &panel);

    draw_text_5x7(renderer, panel.x + 8, panel.y + 6, "MODE:", 2, 215, 220, 235, 255);

    const uint8_t active_r = 180, active_g = 240, active_b = 220;
    const uint8_t idle_r = 150, idle_g = 155, idle_b = 165;

    const int m = (int)app_state->view_mode;
    draw_text_5x7(renderer, panel.x + 8, panel.y + 22, "1 WAVEFORM", 2,
                  (m == DATALAB_VIEW_SPEED) ? active_r : idle_r,
                  (m == DATALAB_VIEW_SPEED) ? active_g : idle_g,
                  (m == DATALAB_VIEW_SPEED) ? active_b : idle_b,
                  255);
    draw_text_5x7(renderer, panel.x + 8, panel.y + 38, "2 WAVEFORM+MARKERS", 1,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_r : idle_r,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_g : idle_g,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_b : idle_b,
                  255);
    draw_text_5x7(renderer, panel.x + 8, panel.y + 50, "3 MARKERS", 1,
                  (m == DATALAB_VIEW_DENSITY) ? active_r : idle_r,
                  (m == DATALAB_VIEW_DENSITY) ? active_g : idle_g,
                  (m == DATALAB_VIEW_DENSITY) ? active_b : idle_b,
                  255);
}

static void draw_daw_markers(SDL_Renderer *renderer, const DatalabFrame *frame, const SDL_Rect *plot_rect) {
    if (!renderer || !frame || !plot_rect || frame->marker_count == 0 || plot_rect->w <= 1) {
        return;
    }

    uint64_t start = frame->start_frame;
    uint64_t end = frame->end_frame;
    uint64_t span = end > start ? (end - start) : 1;

    for (size_t i = 0; i < frame->marker_count; ++i) {
        const DatalabDawMarker *m = &frame->markers[i];
        if (m->frame < start || m->frame > end) {
            continue;
        }

        double ratio = (double)(m->frame - start) / (double)span;
        int x = plot_rect->x + (int)(ratio * (double)(plot_rect->w - 1));

        switch (m->kind) {
            case 1: SDL_SetRenderDrawColor(renderer, 255, 180, 70, 220); break;   // tempo
            case 2: SDL_SetRenderDrawColor(renderer, 120, 210, 255, 220); break;  // time signature
            case 3: SDL_SetRenderDrawColor(renderer, 110, 255, 110, 230); break;  // loop start
            case 4: SDL_SetRenderDrawColor(renderer, 255, 110, 110, 230); break;  // loop end
            case 5: SDL_SetRenderDrawColor(renderer, 230, 230, 230, 210); break;  // playhead
            default: SDL_SetRenderDrawColor(renderer, 190, 190, 190, 180); break;
        }

        SDL_RenderDrawLine(renderer, x, plot_rect->y, x, plot_rect->y + plot_rect->h - 1);
    }
}

static void render_daw_frame(SDL_Renderer *renderer, const DatalabFrame *frame, const DatalabAppState *app_state) {
    if (!renderer || !frame || !app_state || !frame->wave_min || !frame->wave_max || frame->point_count == 0) {
        return;
    }

    int ww = 0;
    int wh = 0;
    SDL_GetRendererOutputSize(renderer, &ww, &wh);

    SDL_Rect plot = { 30, 40, ww - 60, wh - 80 };
    if (plot.w < 10 || plot.h < 10) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 12, 13, 18, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 28, 31, 40, 255);
    SDL_RenderFillRect(renderer, &plot);

    const int center_y = plot.y + (plot.h / 2);
    const float amp = (float)plot.h * 0.46f;

    SDL_SetRenderDrawColor(renderer, 60, 65, 80, 255);
    SDL_RenderDrawLine(renderer, plot.x, center_y, plot.x + plot.w - 1, center_y);

    if (app_state->view_mode != DATALAB_VIEW_DENSITY) {
        SDL_SetRenderDrawColor(renderer, 180, 240, 220, 255);
        for (int x = 0; x < plot.w; ++x) {
            uint64_t idx = (uint64_t)((double)x * (double)(frame->point_count - 1) / (double)(plot.w - 1));
            float min_v = clamp_unit(frame->wave_min[idx]);
            float max_v = clamp_unit(frame->wave_max[idx]);

            int y_top = center_y - (int)(max_v * amp);
            int y_bottom = center_y - (int)(min_v * amp);
            int px = plot.x + x;
            SDL_RenderDrawLine(renderer, px, y_top, px, y_bottom);
        }
    }

    if (app_state->view_mode != DATALAB_VIEW_SPEED) {
        draw_daw_markers(renderer, frame, &plot);
    }

    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 255);
    SDL_RenderDrawRect(renderer, &plot);
    draw_daw_legend(renderer, app_state, &plot);
}

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
        draw_text_5x7(renderer, x - 14, label_y, label, 1, 120, 130, 150, 255);
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
            label_y = clamp_int(y - 4, band->y + 1, band->y + band->h - 8);
            draw_text_5x7(renderer, band->x + band->w - 34, label_y, label, 1, 120, 130, 150, 255);
        }
    }
}

static void draw_trace_legend(SDL_Renderer *renderer, const TraceLaneMeta *lanes, size_t lane_count, const SDL_Rect *header_rect) {
    if (!renderer || !lanes || !header_rect) return;

    SDL_Rect panel = {
        header_rect->x + 8,
        header_rect->y + 6,
        270,
        header_rect->h - 12
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 182);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 220);
    SDL_RenderDrawRect(renderer, &panel);

    draw_text_5x7(renderer, panel.x + 8, panel.y + 4, "TRACE LANES", 1, 215, 220, 235, 255);
    for (size_t i = 0; i < lane_count; ++i) {
        SDL_Color c = trace_lane_color(i);
        int y = panel.y + 14 + (int)i * 10;
        if (y > panel.y + panel.h - 10) break;
        SDL_Rect swatch = { panel.x + 8, y + 1, 10, 6 };
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(renderer, &swatch);
        draw_text_5x7(renderer, panel.x + 24, y, lanes[i].lane, 1, 205, 210, 220, 255);
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
        char line[64];
        char summary[96];
        char marker_line[112];
        SDL_Rect panel = {
            header_rect->x + header_rect->w - 388,
            header_rect->y + 6,
            380,
            header_rect->h - 12
        };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 8, 10, 16, 182);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 95, 110, 220);
        SDL_RenderDrawRect(renderer, &panel);

        snprintf(line, sizeof(line), "T %.4f", play_t);
        draw_text_5x7(renderer, panel.x + 8, panel.y + 4, line, 1, 230, 230, 235, 255);
        snprintf(summary,
                 sizeof(summary),
                 "S:%zu M:%zu DUR:%.4f",
                 total_samples,
                 total_markers,
                 (max_t > min_t) ? (max_t - min_t) : 0.0);
        draw_text_5x7(renderer, panel.x + 112, panel.y + 4, summary, 1, 180, 190, 205, 255);

        if (nearest_marker_label && nearest_marker_label[0] != '\0') {
            snprintf(marker_line, sizeof(marker_line), "MKR %.4f %s", nearest_marker_t, nearest_marker_label);
        } else {
            snprintf(marker_line, sizeof(marker_line), "MKR N/A");
        }
        draw_text_5x7(renderer, panel.x + 8, panel.y + 14, marker_line, 1, 250, 180, 120, 255);

        for (size_t i = 0; i < lane_count; ++i) {
            SDL_Color c = trace_lane_color(i);
            int y = panel.y + 24 + (int)i * 10;
            if (y > panel.y + panel.h - 10) break;
            SDL_Rect swatch = { panel.x + 8, y + 1, 10, 6 };
            char value_line[96];
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(renderer, &swatch);
            if (lanes[i].has_current_value) {
                snprintf(value_line, sizeof(value_line), "%s: %.4f", lanes[i].lane, lanes[i].current_value);
            } else {
                snprintf(value_line, sizeof(value_line), "%s: N/A", lanes[i].lane);
            }
            draw_text_5x7(renderer, panel.x + 24, y, value_line, 1, 215, 220, 235, 255);
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
    header.x = 32;
    header.y = 8;
    header.w = ww - 64;
    header.h = 64;
    plot.x = 32;
    plot.y = header.y + header.h + 6;
    plot.w = ww - 64;
    plot.h = wh - plot.y - 42;
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
        SDL_Rect band = { plot.x + 8, by + 4, plot.w - 16, bh - 8 };
        if (band.h < 12) continue;

        SDL_SetRenderDrawColor(renderer, 31, 35, 46, 255);
        SDL_RenderFillRect(renderer, &band);
        SDL_SetRenderDrawColor(renderer, 64, 70, 86, 255);
        SDL_RenderDrawRect(renderer, &band);
        draw_trace_value_grid(renderer, &band, lanes[l].min_v, lanes[l].max_v, &grid);

        draw_text_5x7(renderer, band.x + 6, band.y + 6, lanes[l].lane, 1, 170, 220, 240, 255);

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
    draw_text_5x7(renderer, plot.x + 10, trace_time_label_y + 10, "TIME SECONDS", 1, 120, 130, 150, 255);
    draw_text_5x7(renderer, plot.x + 120, trace_time_label_y + 10, "GRID T=SECONDS V=VALUE", 1, 120, 130, 150, 255);

    footer_y = wh - 14;
    draw_text_5x7(renderer, plot.x + 10, footer_y, "LEFT RIGHT SCRUB", 1, 205, 210, 220, 255);
    draw_text_5x7(renderer, plot.x + 180, footer_y, "Z ZOOM-STUB", 1, 160, 175, 190, 255);
    draw_text_5x7(renderer, plot.x + 286, footer_y, "X STATS-STUB", 1, 160, 175, 190, 255);
    draw_text_5x7(renderer, plot.x + 392, footer_y, "C LANE-CYCLE", 1, 160, 175, 190, 255);

    core_free(time_points);
}

static CoreResult render_physics_loop(SDL_Window *window,
                                      SDL_Renderer *renderer,
                                      const DatalabFrame *frame,
                                      DatalabAppState *app_state) {
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             (int)frame->width,
                                             (int)frame->height);
    if (!texture) {
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }

    const size_t sample_count = (size_t)frame->width * (size_t)frame->height;
    const size_t rgba_size = sample_count * 4u;

    uint8_t *density_rgba = (uint8_t *)core_alloc(rgba_size);
    uint8_t *speed_rgba = (uint8_t *)core_alloc(rgba_size);
    float *speed = (float *)core_alloc(sample_count * sizeof(float));
    KitVizVecSegment *segments = (KitVizVecSegment *)core_alloc(sample_count * sizeof(KitVizVecSegment));
    if (!density_rgba || !speed_rgba || !speed || !segments) {
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    KitVizFieldStats dens_stats;
    CoreResult rs = kit_viz_compute_field_stats(frame->density, frame->width, frame->height, &dens_stats);
    if (rs.code != CORE_OK) {
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
    }

    for (size_t i = 0; i < sample_count; ++i) {
        const float x = frame->velx[i];
        const float y = frame->vely[i];
        speed[i] = sqrtf((x * x) + (y * y));
    }

    KitVizFieldStats speed_stats;
    rs = kit_viz_compute_field_stats(speed, frame->width, frame->height, &speed_stats);
    if (rs.code != CORE_OK) {
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
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
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
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
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
    }

    int quit = 0;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type == SDL_KEYDOWN) datalab_handle_keydown(&e.key, app_state, &quit);
        }

        const uint8_t *pixels = density_rgba;
        if (app_state->view_mode == DATALAB_VIEW_SPEED) {
            pixels = speed_rgba;
        }

        SDL_UpdateTexture(texture, NULL, pixels, (int)frame->width * 4);

        int ww = 0;
        int wh = 0;
        SDL_GetRendererOutputSize(renderer, &ww, &wh);

        SDL_Rect dst;
        calc_fit_rect(ww, wh, frame->width, frame->height, &dst);

        SDL_SetRenderDrawColor(renderer, 10, 10, 14, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &dst);

        if (app_state->view_mode == DATALAB_VIEW_DENSITY_VECTOR) {
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
                SDL_SetRenderDrawColor(renderer, 230, 255, 230, 255);
                for (size_t i = 0; i < seg_count; ++i) {
                    const float x0 = (float)dst.x + (segments[i].x0 / (float)frame->width) * (float)dst.w;
                    const float y0 = (float)dst.y + (segments[i].y0 / (float)frame->height) * (float)dst.h;
                    const float x1 = (float)dst.x + (segments[i].x1 / (float)frame->width) * (float)dst.w;
                    const float y1 = (float)dst.y + (segments[i].y1 / (float)frame->height) * (float)dst.h;
                    SDL_RenderDrawLine(renderer, (int)x0, (int)y0, (int)x1, (int)y1);
                }
            }
        }

        char title[256];
        make_title(frame, app_state, title, sizeof(title));
        SDL_SetWindowTitle(window, title);

        SDL_RenderPresent(renderer);
    }

    core_free(density_rgba);
    core_free(speed_rgba);
    core_free(speed);
    core_free(segments);
    SDL_DestroyTexture(texture);
    return core_result_ok();
}

static CoreResult render_daw_loop(SDL_Window *window,
                                  SDL_Renderer *renderer,
                                  const DatalabFrame *frame,
                                  DatalabAppState *app_state) {
    int quit = 0;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            if (e.type == SDL_KEYDOWN) {
                datalab_handle_keydown(&e.key, app_state, &quit);
            }
        }

        render_daw_frame(renderer, frame, app_state);

        char title[256];
        make_title(frame, app_state, title, sizeof(title));
        SDL_SetWindowTitle(window, title);

        SDL_RenderPresent(renderer);
    }

    return core_result_ok();
}

static CoreResult render_trace_loop(SDL_Window *window,
                                    SDL_Renderer *renderer,
                                    const DatalabFrame *frame,
                                    DatalabAppState *app_state) {
    int quit = 0;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            if (e.type == SDL_KEYDOWN) {
                datalab_handle_keydown(&e.key, app_state, &quit);
            }
        }

        render_trace_frame(renderer, frame, app_state);
        {
            char title[256];
            make_title(frame, app_state, title, sizeof(title));
            SDL_SetWindowTitle(window, title);
        }
        SDL_RenderPresent(renderer);
    }
    return core_result_ok();
}

CoreResult datalab_render_run(const DatalabFrame *frame, DatalabAppState *app_state) {
    if (!frame || !app_state) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (frame->profile == DATALAB_PROFILE_PHYSICS) {
        if (!frame->density || !frame->velx || !frame->vely || frame->width == 0 || frame->height == 0) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid physics frame" };
            return r;
        }
    } else if (frame->profile == DATALAB_PROFILE_DAW) {
        if (!frame->wave_min || !frame->wave_max || frame->point_count == 0) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid daw frame" };
            return r;
        }
    } else if (frame->profile == DATALAB_PROFILE_TRACE) {
        if (!frame->trace_samples || frame->trace_sample_count == 0) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid trace frame" };
            return r;
        }
    } else {
        CoreResult r = { CORE_ERR_INVALID_ARG, "unknown frame profile" };
        return r;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }

    SDL_Window *window = SDL_CreateWindow("DataLab",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          1200,
                                          900,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window,
                                                -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }

    CoreResult run_r;
    if (frame->profile == DATALAB_PROFILE_DAW) {
        run_r = render_daw_loop(window, renderer, frame, app_state);
    } else if (frame->profile == DATALAB_PROFILE_TRACE) {
        run_r = render_trace_loop(window, renderer, frame, app_state);
    } else {
        run_r = render_physics_loop(window, renderer, frame, app_state);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return run_r;
}
