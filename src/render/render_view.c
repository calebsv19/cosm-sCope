#include "render/render_view.h"
#include "render/render_view_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui/input.h"

static int datalab_env_flag_enabled(const char *name) {
    const char *value = NULL;
    if (!name) {
        return 0;
    }
    value = getenv(name);
    if (!value || !value[0]) {
        return 0;
    }
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

int datalab_ir1_diag_enabled(void) {
    return datalab_env_flag_enabled("DATALAB_IR1_DIAG");
}

int datalab_rs1_diag_enabled(void) {
    return datalab_env_flag_enabled("DATALAB_RS1_DIAG");
}

static const char *daw_view_mode_name(DatalabViewMode mode) {
    switch (mode) {
        case DATALAB_VIEW_DENSITY: return "markers";
        case DATALAB_VIEW_SPEED: return "waveform";
        case DATALAB_VIEW_DENSITY_VECTOR: return "waveform+markers";
        default: return "unknown";
    }
}

int lane_name_eq(const char *a, const char *b) { return strncmp(a, b, 31) == 0; }

void make_title(const DatalabFrame *frame, const DatalabAppState *state, char *title, size_t title_cap) {
    const char *authoring_overlay = NULL;
    if (!frame || !state || !title || title_cap == 0) return;
    authoring_overlay = datalab_workspace_authoring_overlay_mode_name(state->workspace_authoring_overlay_mode);

    if (frame->profile == DATALAB_PROFILE_DAW) {
        snprintf(title,
                 title_cap,
                 "DataLab | DAW points=%llu markers=%zu sr=%u mode=%s text=%+d auth=%s/%s pending=%u entry=%u",
                 (unsigned long long)frame->point_count,
                 frame->marker_count,
                 frame->sample_rate,
                 daw_view_mode_name(state->view_mode),
                 state->text_zoom_step,
                 state->workspace_authoring_stub_active ? "on" : "off",
                 authoring_overlay,
                 (unsigned int)state->workspace_authoring_pending_stub,
                 (unsigned int)state->workspace_authoring_entry_count);
        return;
    }

    if (frame->profile == DATALAB_PROFILE_TRACE) {
        size_t cursor = state->trace_cursor_index;
        if (frame->trace_sample_count == 0) cursor = 0;
        snprintf(title,
                 title_cap,
                 "DataLab | TRACE samples=%zu markers=%zu cursor=%zu zoom_stub=%.2f stats_stub=%s text=%+d auth=%s/%s pending=%u entry=%u",
                 frame->trace_sample_count,
                 frame->trace_marker_count,
                 cursor,
                 state->trace_zoom_stub,
                 state->trace_selection_stub_active ? "on" : "off",
                 state->text_zoom_step,
                 state->workspace_authoring_stub_active ? "on" : "off",
                 authoring_overlay,
                 (unsigned int)state->workspace_authoring_pending_stub,
                 (unsigned int)state->workspace_authoring_entry_count);
        return;
    }

    if (frame->profile == DATALAB_PROFILE_SKETCH) {
        snprintf(title,
                 title_cap,
                 "DataLab | SKETCH raster=%ux%u logical=%ux%u layers=%u objects=%u rendered=%u unsupported=%u text=%+d auth=%s/%s pending=%u entry=%u",
                 frame->width,
                 frame->height,
                 frame->logical_width,
                 frame->logical_height,
                 frame->drawing_layer_count,
                 frame->drawing_object_count,
                 frame->drawing_rendered_object_count,
                 frame->drawing_unsupported_object_count,
                 state->text_zoom_step,
                 state->workspace_authoring_stub_active ? "on" : "off",
                 authoring_overlay,
                 (unsigned int)state->workspace_authoring_pending_stub,
                 (unsigned int)state->workspace_authoring_entry_count);
        return;
    }

    snprintf(title,
             title_cap,
             "DataLab | frame=%llu grid=%ux%u t=%.3f dt=%.3f mode=%s stride=%u text=%+d auth=%s/%s pending=%u entry=%u",
             (unsigned long long)frame->frame_index,
             frame->width,
             frame->height,
             frame->time_seconds,
             frame->dt_seconds,
             datalab_view_mode_name(state->view_mode),
             state->vector_stride,
             state->text_zoom_step,
             state->workspace_authoring_stub_active ? "on" : "off",
             authoring_overlay,
             (unsigned int)state->workspace_authoring_pending_stub,
             (unsigned int)state->workspace_authoring_entry_count);
}

void calc_fit_rect(int ww, int wh, uint32_t fw, uint32_t fh, SDL_Rect *out_rect) {
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

float clamp_unit(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
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
    } else if (frame->profile == DATALAB_PROFILE_SKETCH) {
        if (!frame->drawing_rgba || frame->width == 0 || frame->height == 0) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid sketch frame" };
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
    (void)datalab_text_renderer_init();

    CoreResult run_r;
    if (frame->profile == DATALAB_PROFILE_DAW) {
        run_r = render_daw_loop(window, renderer, frame, app_state);
    } else if (frame->profile == DATALAB_PROFILE_TRACE) {
        run_r = render_trace_loop(window, renderer, frame, app_state);
    } else if (frame->profile == DATALAB_PROFILE_SKETCH) {
        run_r = render_sketch_loop(window, renderer, frame, app_state);
    } else {
        run_r = render_physics_loop(window, renderer, frame, app_state);
    }

    datalab_text_renderer_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return run_r;
}
