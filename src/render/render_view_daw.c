#include "render/render_view_internal.h"

static void draw_daw_legend(SDL_Renderer *renderer, const DatalabAppState *app_state, const SDL_Rect *plot_rect) {
    static const char *kTitle = "MODE";
    static const char *kRow1 = "1 WAVEFORM";
    static const char *kRow2 = "2 WAVE+MARKERS";
    static const char *kRow3 = "3 MARKERS";
    int title_w = 0;
    int title_h = 0;
    int row1_w = 0;
    int row1_h = 0;
    int row2_w = 0;
    int row2_h = 0;
    int row3_w = 0;
    int row3_h = 0;
    int inner_pad = 0;
    int row_gap = 0;
    int max_row_w = 0;
    int panel_w = 0;
    int panel_h = 0;
    int cursor_y = 0;
    SDL_Rect panel;
    if (!renderer || !app_state || !plot_rect) {
        return;
    }
    (void)datalab_measure_text(2, kTitle, &title_w, &title_h);
    (void)datalab_measure_text(2, kRow1, &row1_w, &row1_h);
    (void)datalab_measure_text(1, kRow2, &row2_w, &row2_h);
    (void)datalab_measure_text(1, kRow3, &row3_w, &row3_h);

    inner_pad = datalab_scaled_px(8.0f);
    row_gap = datalab_scaled_px(4.0f);
    max_row_w = row1_w;
    if (row2_w > max_row_w) {
        max_row_w = row2_w;
    }
    if (row3_w > max_row_w) {
        max_row_w = row3_w;
    }

    panel_w = max_row_w + (inner_pad * 2);
    if (title_w + (inner_pad * 2) > panel_w) {
        panel_w = title_w + (inner_pad * 2);
    }
    if (panel_w < datalab_scaled_px(180.0f)) {
        panel_w = datalab_scaled_px(180.0f);
    }
    if (panel_w > (plot_rect->w / 2)) {
        panel_w = plot_rect->w / 2;
    }

    panel_h = inner_pad + title_h + row_gap + row1_h + row_gap + row2_h + row_gap + row3_h + inner_pad;
    if (panel_h < datalab_scaled_px(66.0f)) {
        panel_h = datalab_scaled_px(66.0f);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    panel = (SDL_Rect){
        plot_rect->x + datalab_scaled_px(8.0f),
        plot_rect->y + datalab_scaled_px(8.0f),
        panel_w,
        panel_h
    };
    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 180);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 210);
    SDL_RenderDrawRect(renderer, &panel);

    draw_text_5x7(renderer,
                  panel.x + inner_pad,
                  panel.y + inner_pad,
                  kTitle,
                  2,
                  215,
                  220,
                  235,
                  255);

    const uint8_t active_r = 180, active_g = 240, active_b = 220;
    const uint8_t idle_r = 150, idle_g = 155, idle_b = 165;

    const int m = (int)app_state->view_mode;
    cursor_y = panel.y + inner_pad + title_h + row_gap;
    draw_text_5x7(renderer, panel.x + inner_pad, cursor_y, kRow1, 2,
                  (m == DATALAB_VIEW_SPEED) ? active_r : idle_r,
                  (m == DATALAB_VIEW_SPEED) ? active_g : idle_g,
                  (m == DATALAB_VIEW_SPEED) ? active_b : idle_b,
                  255);
    cursor_y += row1_h + row_gap;
    draw_text_5x7(renderer, panel.x + inner_pad, cursor_y, kRow2, 1,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_r : idle_r,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_g : idle_g,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_b : idle_b,
                  255);
    cursor_y += row2_h + row_gap;
    draw_text_5x7(renderer, panel.x + inner_pad, cursor_y, kRow3, 1,
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
            case 1: SDL_SetRenderDrawColor(renderer, 255, 180, 70, 220); break;
            case 2: SDL_SetRenderDrawColor(renderer, 120, 210, 255, 220); break;
            case 3: SDL_SetRenderDrawColor(renderer, 110, 255, 110, 230); break;
            case 4: SDL_SetRenderDrawColor(renderer, 255, 110, 110, 230); break;
            case 5: SDL_SetRenderDrawColor(renderer, 230, 230, 230, 210); break;
            default: SDL_SetRenderDrawColor(renderer, 190, 190, 190, 180); break;
        }

        SDL_RenderDrawLine(renderer, x, plot_rect->y, x, plot_rect->y + plot_rect->h - 1);
    }
}

void render_daw_frame(SDL_Renderer *renderer, const DatalabFrame *frame, const DatalabAppState *app_state) {
    if (!renderer || !frame || !app_state || !frame->wave_min || !frame->wave_max || frame->point_count == 0) {
        return;
    }
    datalab_sync_text_zoom(app_state);

    int ww = 0;
    int wh = 0;
    SDL_GetRendererOutputSize(renderer, &ww, &wh);

    SDL_Rect plot = {
        datalab_scaled_px(30.0f),
        datalab_scaled_px(40.0f),
        ww - datalab_scaled_px(60.0f),
        wh - datalab_scaled_px(80.0f)
    };
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
