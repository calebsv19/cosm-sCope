#include "ui/input.h"

#include <math.h>

static int datalab_zoom_modifier_active(SDL_Keymod mods) {
    return ((mods & KMOD_CTRL) != 0) || ((mods & KMOD_GUI) != 0);
}

static int datalab_map_window_to_renderer_point(SDL_Window *window,
                                                SDL_Renderer *renderer,
                                                int window_x,
                                                int window_y,
                                                int *out_render_x,
                                                int *out_render_y) {
    int window_w = 0;
    int window_h = 0;
    int render_w = 0;
    int render_h = 0;
    if (!window || !renderer || !out_render_x || !out_render_y) {
        return 0;
    }
    SDL_GetWindowSize(window, &window_w, &window_h);
    SDL_GetRendererOutputSize(renderer, &render_w, &render_h);
    if (window_w <= 0 || window_h <= 0 || render_w <= 0 || render_h <= 0) {
        return 0;
    }
    *out_render_x = (int)lroundf(((float)window_x / (float)window_w) * (float)render_w);
    *out_render_y = (int)lroundf(((float)window_y / (float)window_h) * (float)render_h);
    return 1;
}

int datalab_handle_mouse_event(SDL_Window *window,
                               SDL_Renderer *renderer,
                               const SDL_Event *event,
                               DatalabAppState *state) {
    DatalabRasterViewportState *viewport_state = NULL;
    int render_x = 0;
    int render_y = 0;
    if (!window || !renderer || !event || !state) {
        return 0;
    }
    if (!datalab_profile_supports_raster_viewport(state->profile) || state->workspace_authoring_stub_active) {
        return 0;
    }
    viewport_state = &state->raster_viewport;
    switch (event->type) {
        case SDL_MOUSEWHEEL: {
            int mouse_x = 0;
            int mouse_y = 0;
            float zoom_factor = 1.0f;
            if (!viewport_state->valid) {
                return 0;
            }
            SDL_GetMouseState(&mouse_x, &mouse_y);
            if (!datalab_map_window_to_renderer_point(window, renderer, mouse_x, mouse_y, &render_x, &render_y)) {
                return 0;
            }
            zoom_factor = powf(1.15f, (float)event->wheel.y);
            if (zoom_factor <= 0.0f) {
                return 0;
            }
            if (core_viewport2d_zoom_at_screen_anchor(&viewport_state->viewport,
                                                      (float)render_x,
                                                      (float)render_y,
                                                      zoom_factor)
                .code != CORE_OK) {
                return 0;
            }
            viewport_state->fit_mode = 0;
            viewport_state->reset_requested = 0;
            viewport_state->valid = 1;
            viewport_state->drag_active = 0;
            return 1;
        }
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button != SDL_BUTTON_LEFT || !viewport_state->valid) {
                return 0;
            }
            if (!datalab_map_window_to_renderer_point(window,
                                                      renderer,
                                                      event->button.x,
                                                      event->button.y,
                                                      &render_x,
                                                      &render_y)) {
                return 0;
            }
            viewport_state->drag_active = 1;
            viewport_state->last_mouse_x = render_x;
            viewport_state->last_mouse_y = render_y;
            return 1;
        case SDL_MOUSEBUTTONUP:
            if (event->button.button != SDL_BUTTON_LEFT) {
                return 0;
            }
            viewport_state->drag_active = 0;
            return 1;
        case SDL_MOUSEMOTION:
            if (!viewport_state->drag_active || (event->motion.state & SDL_BUTTON_LMASK) == 0 || !viewport_state->valid) {
                return 0;
            }
            if (!datalab_map_window_to_renderer_point(window,
                                                      renderer,
                                                      event->motion.x,
                                                      event->motion.y,
                                                      &render_x,
                                                      &render_y)) {
                return 0;
            }
            if (core_viewport2d_pan_by(&viewport_state->viewport,
                                       (float)(render_x - viewport_state->last_mouse_x),
                                       (float)(render_y - viewport_state->last_mouse_y))
                .code != CORE_OK) {
                viewport_state->drag_active = 0;
                return 0;
            }
            viewport_state->fit_mode = 0;
            viewport_state->reset_requested = 0;
            viewport_state->valid = 1;
            viewport_state->last_mouse_x = render_x;
            viewport_state->last_mouse_y = render_y;
            return 1;
        default:
            return 0;
    }
}

void datalab_handle_keydown(const SDL_KeyboardEvent *key, DatalabAppState *state, int *quit) {
    if (!key || !state || !quit) return;

    if (datalab_zoom_modifier_active((SDL_Keymod)key->keysym.mod)) {
        switch (key->keysym.sym) {
            case SDLK_EQUALS:
            case SDLK_PLUS:
            case SDLK_KP_PLUS:
                state->text_zoom_step = datalab_text_zoom_step_clamp(state->text_zoom_step + 1);
                return;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                state->text_zoom_step = datalab_text_zoom_step_clamp(state->text_zoom_step - 1);
                return;
            case SDLK_0:
            case SDLK_KP_0:
                state->text_zoom_step = 0;
                return;
            default:
                break;
        }
    }

    switch (key->keysym.sym) {
        case SDLK_ESCAPE:
            *quit = 1;
            break;
        case SDLK_1:
            if (state->profile == DATALAB_PROFILE_DAW) {
                state->view_mode = DATALAB_VIEW_SPEED; /* waveform */
            } else {
                state->view_mode = DATALAB_VIEW_DENSITY;
            }
            break;
        case SDLK_2:
            if (state->profile == DATALAB_PROFILE_DAW) {
                state->view_mode = DATALAB_VIEW_DENSITY_VECTOR; /* waveform + markers */
            } else {
                state->view_mode = DATALAB_VIEW_SPEED;
            }
            break;
        case SDLK_3:
            if (state->profile == DATALAB_PROFILE_DAW) {
                state->view_mode = DATALAB_VIEW_DENSITY; /* markers */
            } else {
                state->view_mode = DATALAB_VIEW_DENSITY_VECTOR;
            }
            break;
        case SDLK_LEFTBRACKET:
            if (state->profile == DATALAB_PROFILE_PHYSICS && state->vector_stride > 1) state->vector_stride--;
            break;
        case SDLK_RIGHTBRACKET:
            if (state->profile == DATALAB_PROFILE_PHYSICS && state->vector_stride < 64) state->vector_stride++;
            break;
        case SDLK_LEFT:
            if (state->profile == DATALAB_PROFILE_TRACE && state->trace_cursor_index > 0u) {
                state->trace_cursor_index--;
            } else if (state->profile == DATALAB_PROFILE_IMAGE) {
                state->panel_selection_delta -= 1;
                state->panel_open_selected_requested = 1;
            }
            break;
        case SDLK_RIGHT:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                state->trace_cursor_index++;
            } else if (state->profile == DATALAB_PROFILE_IMAGE) {
                state->panel_selection_delta += 1;
                state->panel_open_selected_requested = 1;
            }
            break;
        case SDLK_HOME:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                state->trace_cursor_index = 0u;
            }
            break;
        case SDLK_END:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                state->trace_cursor_index = (size_t)-1;
            }
            break;
        case SDLK_z:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                state->trace_zoom_stub += 0.25f;
                if (state->trace_zoom_stub > 4.0f) state->trace_zoom_stub = 1.0f;
            }
            break;
        case SDLK_x:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                state->trace_selection_stub_active = !state->trace_selection_stub_active;
            }
            break;
        case SDLK_c:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                state->trace_lane_cycle_requested = 1;
            }
            break;
        case SDLK_r:
            state->vector_stride = 8;
            state->vector_scale = 0.15f;
            state->view_mode = (state->profile == DATALAB_PROFILE_DAW) ? DATALAB_VIEW_SPEED : DATALAB_VIEW_DENSITY;
            state->trace_cursor_index = 0u;
            state->trace_zoom_stub = 1.0f;
            state->trace_selection_stub_active = 0;
            state->trace_lane_visibility_index = 0u;
            state->trace_lane_cycle_requested = 0;
            state->panel_rescan_requested = 0;
            state->panel_selection_delta = 0;
            state->panel_open_selected_requested = 0;
            state->panel_requested_pack_path[0] = '\0';
            state->playback_active = 0;
            state->playback_last_advance_ticks = 0u;
            datalab_raster_viewport_request_reset(&state->raster_viewport);
            break;
        case SDLK_o:
            state->open_picker_requested = 1;
            break;
        case SDLK_F5:
            state->panel_rescan_requested = 1;
            break;
        case SDLK_u:
            state->panel_selection_delta -= 1;
            break;
        case SDLK_j:
            state->panel_selection_delta += 1;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            state->panel_open_selected_requested = 1;
            break;
        case SDLK_SPACE:
            state->playback_active = !state->playback_active;
            if (state->playback_interval_ms == 0u) {
                state->playback_interval_ms = DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT;
            }
            if (state->playback_active) {
                state->playback_last_advance_ticks = SDL_GetTicks();
            }
            break;
        case SDLK_h:
            if (!state->workspace_authoring_stub_active) {
                state->session_hud_collapsed = !state->session_hud_collapsed;
            }
            break;
        default:
            break;
    }
}
