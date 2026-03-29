#include "ui/input.h"

static int datalab_zoom_modifier_active(SDL_Keymod mods) {
    return ((mods & KMOD_CTRL) != 0) || ((mods & KMOD_GUI) != 0);
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
                state->view_mode = DATALAB_VIEW_SPEED; // waveform
            } else {
                state->view_mode = DATALAB_VIEW_DENSITY;
            }
            break;
        case SDLK_2:
            if (state->profile == DATALAB_PROFILE_DAW) {
                state->view_mode = DATALAB_VIEW_DENSITY_VECTOR; // waveform + markers
            } else {
                state->view_mode = DATALAB_VIEW_SPEED;
            }
            break;
        case SDLK_3:
            if (state->profile == DATALAB_PROFILE_DAW) {
                state->view_mode = DATALAB_VIEW_DENSITY; // markers
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
            }
            break;
        case SDLK_RIGHT:
            if (state->profile == DATALAB_PROFILE_TRACE) {
                state->trace_cursor_index++;
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
            break;
        default:
            break;
    }
}
