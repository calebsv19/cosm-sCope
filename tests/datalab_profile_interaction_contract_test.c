#include <stdio.h>
#include <string.h>

#include "app/app_state.h"
#include "ui/input.h"

static int datalab_test_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "profile-interaction-contract: %s\n", message ? message : "assertion failed");
        return 0;
    }
    return 1;
}

static void datalab_test_simulate_keydown(DatalabAppState *state, SDL_Keycode sym, Uint16 mod, int *quit) {
    SDL_KeyboardEvent key;
    memset(&key, 0, sizeof(key));
    key.type = SDL_KEYDOWN;
    key.keysym.sym = sym;
    key.keysym.mod = mod;
    datalab_handle_keydown(&key, state, quit);
}

static int test_trace_profile_navigation_and_toggle_contract(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_TRACE);
    state.trace_cursor_index = 2u;
    state.trace_zoom_stub = 4.0f;
    state.trace_selection_stub_active = 0;
    state.trace_lane_cycle_requested = 0;

    datalab_test_simulate_keydown(&state, SDLK_LEFT, 0, &quit);
    if (!datalab_test_assert(state.trace_cursor_index == 1u, "trace left should decrement cursor")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_RIGHT, 0, &quit);
    if (!datalab_test_assert(state.trace_cursor_index == 2u, "trace right should increment cursor")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_HOME, 0, &quit);
    if (!datalab_test_assert(state.trace_cursor_index == 0u, "trace home should reset cursor")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_END, 0, &quit);
    if (!datalab_test_assert(state.trace_cursor_index == (size_t)-1, "trace end should jump to sentinel end")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_z, 0, &quit);
    if (!datalab_test_assert(state.trace_zoom_stub == 1.0f, "trace z should wrap zoom stub after max")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_x, 0, &quit);
    if (!datalab_test_assert(state.trace_selection_stub_active == 1,
                             "trace x should toggle selection stub active")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_c, 0, &quit);
    if (!datalab_test_assert(state.trace_lane_cycle_requested == 1,
                             "trace c should request lane cycle")) {
        return 0;
    }
    if (!datalab_test_assert(quit == 0, "trace profile interactions should not quit runtime")) {
        return 0;
    }
    return 1;
}

static int test_image_profile_arrow_contract(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.bmp", DATALAB_PROFILE_IMAGE);
    state.panel_selection_delta = 0;
    state.panel_open_selected_requested = 0;

    datalab_test_simulate_keydown(&state, SDLK_RIGHT, 0, &quit);
    if (!datalab_test_assert(state.panel_selection_delta == 1,
                             "image right should request next panel item")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_open_selected_requested == 1,
                             "image right should request immediate open")) {
        return 0;
    }

    state.panel_open_selected_requested = 0;
    datalab_test_simulate_keydown(&state, SDLK_LEFT, 0, &quit);
    if (!datalab_test_assert(state.panel_selection_delta == 0,
                             "image left should request previous panel item")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_open_selected_requested == 1,
                             "image left should request immediate open")) {
        return 0;
    }
    if (!datalab_test_assert(quit == 0, "image profile interactions should not quit runtime")) {
        return 0;
    }
    return 1;
}

static int test_physics_stride_and_global_reset_contract(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_PHYSICS);
    state.vector_stride = 1u;

    datalab_test_simulate_keydown(&state, SDLK_LEFTBRACKET, 0, &quit);
    if (!datalab_test_assert(state.vector_stride == 1u,
                             "physics left bracket should clamp vector stride at minimum")) {
        return 0;
    }

    state.vector_stride = 64u;
    datalab_test_simulate_keydown(&state, SDLK_RIGHTBRACKET, 0, &quit);
    if (!datalab_test_assert(state.vector_stride == 64u,
                             "physics right bracket should clamp vector stride at maximum")) {
        return 0;
    }

    state.vector_stride = 12u;
    state.vector_scale = 2.0f;
    state.panel_rescan_requested = 1;
    state.panel_selection_delta = 3;
    state.panel_open_selected_requested = 1;
    snprintf(state.panel_requested_pack_path, sizeof(state.panel_requested_pack_path), "stale.pack");
    state.recent_input_root_dropdown_open = 1;
    state.playback_active = 1;
    state.playback_last_advance_ticks = 123u;
    state.raster_viewport.drag_active = 1;
    state.raster_viewport.fit_mode = 0;
    state.raster_viewport.reset_requested = 0;

    datalab_test_simulate_keydown(&state, SDLK_r, 0, &quit);
    if (!datalab_test_assert(state.vector_stride == 8u && state.vector_scale == 0.15f,
                             "global reset should restore physics vector defaults")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_rescan_requested == 0 &&
                             state.panel_selection_delta == 0 &&
                             state.panel_open_selected_requested == 0 &&
                             state.panel_requested_pack_path[0] == '\0',
                             "global reset should clear panel interaction state")) {
        return 0;
    }
    if (!datalab_test_assert(state.recent_input_root_dropdown_open == 0 &&
                             state.playback_active == 0 &&
                             state.playback_last_advance_ticks == 0u,
                             "global reset should clear dropdown and playback state")) {
        return 0;
    }
    if (!datalab_test_assert(state.raster_viewport.fit_mode == 1 &&
                             state.raster_viewport.reset_requested == 1 &&
                             state.raster_viewport.drag_active == 0,
                             "global reset should request raster viewport reset")) {
        return 0;
    }
    if (!datalab_test_assert(quit == 0, "global reset should not quit runtime")) {
        return 0;
    }
    return 1;
}

static int test_daw_view_mode_contract(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_DAW);

    datalab_test_simulate_keydown(&state, SDLK_1, 0, &quit);
    if (!datalab_test_assert(state.view_mode == DATALAB_VIEW_SPEED,
                             "DAW 1 should select waveform speed view")) {
        return 0;
    }
    datalab_test_simulate_keydown(&state, SDLK_2, 0, &quit);
    if (!datalab_test_assert(state.view_mode == DATALAB_VIEW_DENSITY_VECTOR,
                             "DAW 2 should select waveform plus markers view")) {
        return 0;
    }
    datalab_test_simulate_keydown(&state, SDLK_3, 0, &quit);
    if (!datalab_test_assert(state.view_mode == DATALAB_VIEW_DENSITY,
                             "DAW 3 should select markers-only view")) {
        return 0;
    }
    return 1;
}

static int test_trace_render_state_helper_contract(void) {
    DatalabAppState state;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_TRACE);

    state.trace_cursor_index = (size_t)-1;
    datalab_trace_clamp_cursor_to_count(&state, 4u);
    if (!datalab_test_assert(state.trace_cursor_index == 3u,
                             "trace clamp should resolve sentinel end to the last sample")) {
        return 0;
    }

    state.trace_cursor_index = 99u;
    datalab_trace_clamp_cursor_to_count(&state, 5u);
    if (!datalab_test_assert(state.trace_cursor_index == 4u,
                             "trace clamp should bound oversized cursor to the last sample")) {
        return 0;
    }

    state.trace_lane_visibility_index = 0u;
    state.trace_lane_cycle_requested = 1;
    datalab_trace_apply_lane_cycle(&state, 2u);
    if (!datalab_test_assert(state.trace_lane_visibility_index == 1u &&
                             state.trace_lane_cycle_requested == 0,
                             "trace lane cycle should advance and clear the request")) {
        return 0;
    }

    state.trace_lane_visibility_index = 9u;
    state.trace_lane_cycle_requested = 0;
    datalab_trace_apply_lane_cycle(&state, 2u);
    if (!datalab_test_assert(state.trace_lane_visibility_index == 0u,
                             "trace lane cycle helper should reset out-of-range visibility")) {
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_trace_profile_navigation_and_toggle_contract()) {
        return 1;
    }
    if (!test_image_profile_arrow_contract()) {
        return 1;
    }
    if (!test_physics_stride_and_global_reset_contract()) {
        return 1;
    }
    if (!test_daw_view_mode_contract()) {
        return 1;
    }
    if (!test_trace_render_state_helper_contract()) {
        return 1;
    }
    puts("datalab profile interaction contract test passed");
    return 0;
}
