#include <stdio.h>
#include <string.h>

#include "app/app_state.h"
#include "render/render_view_internal.h"
#include "ui/input.h"

static int datalab_test_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "authoring-contract: %s\n", message ? message : "assertion failed");
        return 0;
    }
    return 1;
}

static void datalab_test_simulate_keydown(DatalabAppState *state, SDL_Keycode sym, Uint16 mod, int *quit) {
    SDL_KeyboardEvent key;
    DatalabWorkspaceAuthoringAdapterResult authoring_route;
    memset(&key, 0, sizeof(key));
    memset(&authoring_route, 0, sizeof(authoring_route));
    key.type = SDL_KEYDOWN;
    key.keysym.sym = sym;
    key.keysym.mod = mod;
    datalab_workspace_authoring_route_keydown(&key, state, &authoring_route);
    if (!authoring_route.consumed) {
        datalab_handle_keydown(&key, state, quit);
    }
}

static int test_authoring_consumes_session_keys(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    state.workspace_authoring_stub_active = 1;
    state.workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state.workspace_authoring_pending_stub = 0u;
    state.session_hud_collapsed = 0;
    state.playback_active = 0;
    state.playback_interval_ms = DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT;
    state.open_picker_requested = 0;
    state.panel_selection_delta = 0;
    state.panel_open_selected_requested = 0;

    datalab_test_simulate_keydown(&state, SDLK_h, 0, &quit);
    if (!datalab_test_assert(state.session_hud_collapsed == 0, "authoring should suppress session HUD toggle")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_SPACE, 0, &quit);
    if (!datalab_test_assert(state.playback_active == 0, "authoring should suppress playback toggle")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_o, 0, &quit);
    if (!datalab_test_assert(state.open_picker_requested == 0, "authoring should suppress picker reopen")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_u, 0, &quit);
    if (!datalab_test_assert(state.panel_selection_delta == 0, "authoring should suppress panel selection movement")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_RETURN, 0, &quit);
    if (!datalab_test_assert(state.panel_open_selected_requested == 0, "authoring should suppress panel open requests")) {
        return 0;
    }

    if (!datalab_test_assert(quit == 0, "authoring session-key suppression should not quit")) {
        return 0;
    }
    return 1;
}

static int test_authoring_tab_apply_cancel_flow(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_PHYSICS);
    state.workspace_authoring_stub_active = 1;
    state.workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state.workspace_authoring_pending_stub = 0u;

    datalab_test_simulate_keydown(&state, SDLK_TAB, 0, &quit);
    if (!datalab_test_assert(state.workspace_authoring_overlay_mode == DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME,
                             "tab should cycle the authoring overlay")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_pending_stub == 1u,
                             "overlay cycle should mark authoring state pending")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_overlay_cycle_count == 1u,
                             "overlay cycle count should increment")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_RETURN, 0, &quit);
    if (!datalab_test_assert(state.workspace_authoring_pending_stub == 0u,
                             "enter should apply and clear pending authoring state")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_apply_count == 1u,
                             "apply count should increment after enter")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_stub_active == 1,
                             "apply should keep authoring active")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_ESCAPE, 0, &quit);
    if (!datalab_test_assert(state.workspace_authoring_stub_active == 0,
                             "escape should exit authoring when popup is closed")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_overlay_mode == DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE,
                             "escape exit should restore pane overlay mode")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_cancel_count == 1u,
                             "cancel count should increment on escape exit")) {
        return 0;
    }
    if (!datalab_test_assert(quit == 0, "escape while authoring should not fall through to runtime quit")) {
        return 0;
    }
    return 1;
}

static int test_authoring_popup_escape_closes_popup_only(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_PHYSICS);
    state.workspace_authoring_stub_active = 1;
    state.workspace_authoring_custom_theme_popup_open = 1u;
    state.workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME;
    state.workspace_authoring_cancel_count = 0u;

    datalab_test_simulate_keydown(&state, SDLK_ESCAPE, 0, &quit);
    if (!datalab_test_assert(state.workspace_authoring_custom_theme_popup_open == 0u,
                             "escape should close the custom theme popup")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_stub_active == 1,
                             "popup escape should keep authoring active")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_cancel_count == 0u,
                             "popup escape should not count as full authoring cancel")) {
        return 0;
    }
    if (!datalab_test_assert(quit == 0, "popup escape should not quit runtime")) {
        return 0;
    }
    return 1;
}

static int test_authoring_disables_session_mouse_controls(void) {
    DatalabAppState state;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);

    if (!datalab_test_assert(datalab_session_controls_mouse_enabled(&state) == 1,
                             "session mouse controls should be enabled outside authoring")) {
        return 0;
    }

    state.workspace_authoring_stub_active = 1;
    if (!datalab_test_assert(datalab_session_controls_mouse_enabled(&state) == 0,
                             "session mouse controls should be disabled while authoring is active")) {
        return 0;
    }

    state.workspace_authoring_custom_theme_popup_open = 1u;
    if (!datalab_test_assert(datalab_session_controls_mouse_enabled(&state) == 0,
                             "custom theme popup should still keep session mouse controls disabled")) {
        return 0;
    }

    return 1;
}

static int test_runtime_theme_cycle_keys(void) {
    DatalabAppState state;
    int quit = 0;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);

    state.workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    datalab_test_simulate_keydown(&state, SDLK_t, KMOD_GUI, &quit);
    if (!datalab_test_assert(state.workspace_authoring_theme_preset_id ==
                                 (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT,
                             "cmd/ctrl+t should cycle to the next runtime theme")) {
        return 0;
    }

    datalab_test_simulate_keydown(&state, SDLK_t, KMOD_GUI | KMOD_SHIFT, &quit);
    if (!datalab_test_assert(state.workspace_authoring_theme_preset_id ==
                                 (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST,
                             "cmd/ctrl+shift+t should cycle to the previous runtime theme")) {
        return 0;
    }

    state.workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    datalab_test_simulate_keydown(&state, SDLK_t, KMOD_CTRL, &quit);
    if (!datalab_test_assert(state.workspace_authoring_theme_preset_id ==
                                 (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT,
                             "runtime theme cycling should leave custom mode through the preset ring")) {
        return 0;
    }
    if (!datalab_test_assert(quit == 0, "theme cycling should not quit")) {
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_authoring_consumes_session_keys()) {
        return 1;
    }
    if (!test_authoring_tab_apply_cancel_flow()) {
        return 1;
    }
    if (!test_authoring_popup_escape_closes_popup_only()) {
        return 1;
    }
    if (!test_authoring_disables_session_mouse_controls()) {
        return 1;
    }
    if (!test_runtime_theme_cycle_keys()) {
        return 1;
    }
    puts("datalab authoring input contract test passed");
    return 0;
}
