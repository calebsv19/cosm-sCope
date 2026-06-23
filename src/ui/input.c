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
            if (!datalab_raster_viewport_zoom_at_screen_anchor(viewport_state,
                                                               render_x,
                                                               render_y,
                                                               zoom_factor)) {
                return 0;
            }
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
            return datalab_raster_viewport_begin_drag(viewport_state, render_x, render_y);
        case SDL_MOUSEBUTTONUP:
            if (event->button.button != SDL_BUTTON_LEFT) {
                return 0;
            }
            datalab_raster_viewport_end_drag(viewport_state);
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
            return datalab_raster_viewport_drag_to(viewport_state, render_x, render_y);
        default:
            return 0;
    }
}

void datalab_handle_keydown(const SDL_KeyboardEvent *key, DatalabAppState *state, int *quit) {
    if (!key || !state || !quit) return;

    if (datalab_zoom_modifier_active((SDL_Keymod)key->keysym.mod)) {
        switch (key->keysym.sym) {
            case SDLK_t:
                state->workspace_authoring_theme_preset_id =
                    datalab_workspace_authoring_cycle_runtime_theme_preset(
                        state->workspace_authoring_theme_preset_id,
                        ((key->keysym.mod & KMOD_SHIFT) != 0) ? -1 : 1);
                return;
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
            datalab_profile_select_view_slot(state, 1);
            break;
        case SDLK_2:
            datalab_profile_select_view_slot(state, 2);
            break;
        case SDLK_3:
            datalab_profile_select_view_slot(state, 3);
            break;
        case SDLK_LEFTBRACKET:
            datalab_physics_adjust_vector_stride(state, -1);
            break;
        case SDLK_RIGHTBRACKET:
            datalab_physics_adjust_vector_stride(state, 1);
            break;
        case SDLK_LEFT:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                datalab_trace_step_cursor(state, -1);
            } else if (state->profile == DATALAB_PROFILE_IMAGE) {
                datalab_panel_request_step(state, -1, 1);
            }
            break;
        case SDLK_RIGHT:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                datalab_trace_step_cursor(state, 1);
            } else if (state->profile == DATALAB_PROFILE_IMAGE) {
                datalab_panel_request_step(state, 1, 1);
            }
            break;
        case SDLK_HOME:
            datalab_trace_set_cursor_home(state);
            break;
        case SDLK_END:
            datalab_trace_set_cursor_end(state);
            break;
        case SDLK_z:
            datalab_trace_cycle_zoom(state);
            break;
        case SDLK_x:
            datalab_trace_toggle_selection(state);
            break;
        case SDLK_c:
            datalab_trace_request_lane_cycle(state);
            break;
        case SDLK_r:
            datalab_app_state_reset_interactions(state);
            break;
        case SDLK_o:
            datalab_app_state_request_picker(state);
            break;
        case SDLK_F5:
            datalab_app_state_request_panel_rescan(state);
            break;
        case SDLK_u:
            datalab_panel_request_step(state, -1, 0);
            break;
        case SDLK_j:
            datalab_panel_request_step(state, 1, 0);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            datalab_panel_request_open_selected(state);
            break;
        case SDLK_SPACE:
            datalab_playback_toggle_active(state, SDL_GetTicks(), DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT);
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
