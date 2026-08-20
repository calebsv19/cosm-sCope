#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static int test_recipe_runtime_isolation_contract(void) {
    const char *runtime_root = getenv("DATALAB_TEST_RUNTIME_ROOT");
    const char *source_root = getenv("DATALAB_TEST_SOURCE_ROOT");
    char cwd[PATH_MAX];
    struct stat source_info;
    return datalab_test_assert(runtime_root &&
                                   strncmp(runtime_root, "/private/tmp/datalab-authoring-contract.",
                                           strlen("/private/tmp/datalab-authoring-contract.")) == 0,
                               "authoring contract must run from an explicit private temporary runtime") &&
           datalab_test_assert(getcwd(cwd, sizeof(cwd)) && strcmp(cwd, runtime_root) == 0,
                               "authoring contract must never fall back to the source cwd") &&
           datalab_test_assert(source_root && stat(source_root, &source_info) == 0 && S_ISDIR(source_info.st_mode),
                               "authoring contract must retain an explicit source-root context");
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
    datalab_workspace_authoring_begin_takeover(&state);
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
    datalab_workspace_authoring_begin_takeover(&state);

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
    if (!datalab_test_assert(state.workspace_authoring_stub_active == 0,
                             "apply should resume runtime")) {
        return 0;
    }

    datalab_workspace_authoring_begin_takeover(&state);
    datalab_test_simulate_keydown(&state, SDLK_ESCAPE, 0, &quit);
    if (!datalab_test_assert(state.workspace_authoring_stub_active == 0,
                             "escape should restore baseline and resume runtime")) {
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
    datalab_workspace_authoring_begin_takeover(&state);
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

static int test_authoring_takeover_snapshot_helpers(void) {
    DatalabAppState state;
    DatalabWorkspaceCustomTheme original_theme;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    original_theme = state.workspace_authoring_custom_theme;
    state.text_zoom_step = 2;
    state.workspace_authoring_theme_preset_id =
        (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT;
    state.workspace_authoring_custom_theme_active_slot = 0u;
    state.workspace_authoring_custom_theme_slots[0].clear_r = 31u;
    datalab_workspace_authoring_begin_takeover(&state);

    if (!datalab_test_assert(state.workspace_authoring_stub_active == 1,
                             "begin helper should activate authoring")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_entry_count == 1u,
                             "begin helper should count authoring entry")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_entry_text_zoom_step == 2,
                             "begin helper should capture text zoom snapshot")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_entry_theme_preset_id ==
                                 (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT,
                             "begin helper should capture theme preset snapshot")) {
        return 0;
    }

    state.text_zoom_step = 5;
    state.workspace_authoring_theme_preset_id =
        (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_GREYSCALE;
    state.workspace_authoring_custom_theme_slots[0].clear_r = 210u;
    state.workspace_authoring_custom_theme = state.workspace_authoring_custom_theme_slots[0];
    state.workspace_authoring_pending_stub = 1u;
    state.workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME;
    if (!datalab_test_assert(datalab_workspace_authoring_cancel_and_exit(&state) == 1,
                             "cancel helper should report restored text zoom")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_stub_active == 0,
                             "cancel helper should exit authoring")) {
        return 0;
    }
    if (!datalab_test_assert(state.text_zoom_step == 2,
                             "cancel helper should restore text zoom snapshot")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_theme_preset_id ==
                                 (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT,
                             "cancel helper should restore theme preset snapshot")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_custom_theme.clear_r == 31u,
                             "cancel helper should restore custom theme snapshot")) {
        return 0;
    }

    datalab_workspace_authoring_begin_takeover(&state);
    state.text_zoom_step = -1;
    state.workspace_authoring_theme_preset_id =
        (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    state.workspace_authoring_custom_theme_popup_open = 1u;
    state.workspace_authoring_pending_stub = 1u;
    datalab_workspace_authoring_apply_takeover(&state);
    if (!datalab_test_assert(state.workspace_authoring_pending_stub == 0u,
                             "apply helper should clear pending state")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_custom_theme_popup_open == 0u,
                             "apply helper should close custom theme popup")) {
        return 0;
    }
    if (!datalab_test_assert(state.workspace_authoring_entry_text_zoom_step == -1,
                             "apply helper should refresh text zoom baseline")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_workspace_authoring_close_custom_theme_popup(&state) == 0,
                             "close popup helper should be a no-op when closed")) {
        return 0;
    }
    state.workspace_authoring_custom_theme_popup_open = 1u;
    if (!datalab_test_assert(datalab_workspace_authoring_close_custom_theme_popup(&state) == 1,
                             "close popup helper should report closing an open popup")) {
        return 0;
    }

    state.workspace_authoring_custom_theme = original_theme;
    return 1;
}

static int test_authoring_session_failed_safe_recovery_and_shutdown(void) {
    DatalabAppState state;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    state.text_zoom_step = 2;
    datalab_workspace_authoring_begin_takeover(&state);
    state.text_zoom_step = 5;
    state.workspace_authoring_pending_stub = 1u;
    state.workspace_authoring_session.state = CORE_WORKSPACE_AUTHORING_SESSION_FAILED_SAFE;
    datalab_workspace_authoring_recover_failed_safe(&state);
    if (!datalab_test_assert(state.workspace_authoring_session.state == CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME &&
                                 state.workspace_authoring_stub_active == 0 && state.text_zoom_step == 2,
                             "failed-safe recovery should restore the baseline before runtime resumes")) {
        return 0;
    }
    datalab_workspace_authoring_begin_takeover(&state);
    datalab_workspace_authoring_shutdown(&state);
    if (!datalab_test_assert(state.workspace_authoring_session.state == CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME &&
                                 state.workspace_authoring_stub_active == 0,
                             "shutdown should cancel an active authoring draft before runtime teardown")) {
        return 0;
    }
    return 1;
}

static int test_fixed_visualizer_projection_drafts_and_restores(void) {
    DatalabAppState state;
    CorePaneLeafRect rects[2];
    float baseline;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    baseline = state.workspace_authoring_projection.profile_surface_ratio;
    if (!datalab_test_assert(datalab_workspace_authoring_projection_solve(&state, 1000, 700, rects),
                             "fixed visualizer projection should solve its two source-proven surfaces")) return 0;
    if (!datalab_test_assert(rects[0].id == DATALAB_WORKSPACE_SURFACE_PROFILE &&
                                 rects[1].id == DATALAB_WORKSPACE_SURFACE_SOURCE_CONTROLS,
                             "projection should contain only profile canvas and source controls")) return 0;
    datalab_workspace_authoring_begin_takeover(&state);
    if (!datalab_test_assert(datalab_workspace_authoring_projection_apply_drag(&state, 140.0f, 1000.0f),
                             "authoring projection should accept a bounded splitter draft")) return 0;
    if (!datalab_test_assert(state.workspace_authoring_projection.profile_surface_ratio != baseline,
                             "splitter draft should change the source-controls ratio")) return 0;
    (void)datalab_workspace_authoring_cancel_and_exit(&state);
    if (!datalab_test_assert(state.workspace_authoring_projection.profile_surface_ratio == baseline,
                             "cancel should restore the projection entry baseline")) return 0;
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

static int test_authoring_runtime_mutation_gate_covers_direct_paths(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;
    uint32_t baseline_stride = 0u;
    memset(&cache, 0, sizeof(cache));
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_PHYSICS);
    state.panel_rescan_requested = 1;
    state.panel_selection_delta = 3;
    state.playback_active = 1;
    state.playback_speed_index = 2;
    state.playback_mode = DATALAB_PLAYBACK_MODE_LOOP;
    state.open_picker_requested = 0;
    baseline_stride = state.vector_stride;
    datalab_workspace_authoring_begin_takeover(&state);

    datalab_panel_request_step(&state, 4, 1);
    datalab_panel_request_open_selected(&state);
    datalab_panel_request_pack_path(&state, "other.pack");
    datalab_playback_toggle_active(&state, 100u, 100u);
    datalab_playback_set_speed_index(&state, 4, 100u);
    datalab_playback_set_mode(&state, DATALAB_PLAYBACK_MODE_BOUNCE);
    datalab_app_state_request_picker(&state);
    datalab_app_state_request_panel_rescan(&state);
    datalab_profile_select_view_slot(&state, 2);
    datalab_physics_adjust_vector_stride(&state, 4);
    datalab_session_controls_tick(&state);
    datalab_panel_apply_state(&state, &cache, "/tmp", 1, 100u);

    if (!datalab_test_assert(state.panel_rescan_requested == 1,
                             "authoring must freeze an already-queued panel rescan")) return 0;
    if (!datalab_test_assert(state.panel_selection_delta == 3 &&
                                 state.panel_open_selected_requested == 0 &&
                                 state.panel_requested_pack_path[0] == '\0',
                             "authoring must reject direct panel and file mutations")) return 0;
    if (!datalab_test_assert(state.playback_active == 1 &&
                                 state.playback_speed_index == 2 &&
                                 state.playback_mode == DATALAB_PLAYBACK_MODE_LOOP,
                             "authoring must freeze direct playback mutations")) return 0;
    if (!datalab_test_assert(state.open_picker_requested == 0 && state.vector_stride == baseline_stride,
                             "authoring must reject picker and profile mutations")) return 0;
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

static int test_authoring_route_diagnostics_are_normalized(void) {
    DatalabAppState state;
    int quit = 0;
    const DatalabWorkspaceAuthoringRouteDiagnostic *diag = NULL;
    CoreResult result;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_PHYSICS);
    state.workspace_authoring_stub_active = 1;
    state.workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state.workspace_authoring_pending_stub = 0u;

    datalab_workspace_authoring_clear_route_diagnostic();
    datalab_test_simulate_keydown(&state, SDLK_TAB, 0, &quit);
    diag = datalab_workspace_authoring_last_route_diagnostic();
    if (!datalab_test_assert(strcmp(diag->route, "key.trigger") == 0,
                             "tab diagnostic should identify key trigger route")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(diag->action, "workspace.cycle_overlay") == 0,
                             "tab diagnostic should identify normalized action")) {
        return 0;
    }
    if (!datalab_test_assert(diag->result_code == CORE_OK && diag->consumed == 1u,
                             "tab diagnostic should report consumed success")) {
        return 0;
    }
    if (!datalab_test_assert(diag->overlay_before == DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE &&
                             diag->overlay_after == DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME,
                             "tab diagnostic should capture overlay before and after")) {
        return 0;
    }
    if (!datalab_test_assert(diag->pending_before == 0u && diag->pending_after == 1u,
                             "tab diagnostic should capture pending state transition")) {
        return 0;
    }

    state.workspace_authoring_custom_theme_popup_open = 1u;
    datalab_test_simulate_keydown(&state, SDLK_ESCAPE, 0, &quit);
    diag = datalab_workspace_authoring_last_route_diagnostic();
    if (!datalab_test_assert(strcmp(diag->route, "key.popup") == 0,
                             "popup escape diagnostic should identify popup route")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(diag->action, "popup.close") == 0,
                             "popup escape diagnostic should identify close action")) {
        return 0;
    }
    if (!datalab_test_assert(diag->active_before == 1u && diag->active_after == 1u,
                             "popup close should not exit authoring in diagnostics")) {
        return 0;
    }

    result = datalab_workspace_authoring_dispatch_action_for_route(&state,
                                                                   "workspace.unknown",
                                                                   "contract.unsupported");
    diag = datalab_workspace_authoring_last_route_diagnostic();
    if (!datalab_test_assert(result.code != CORE_OK,
                             "unsupported authoring action should fail")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(diag->route, "contract.unsupported") == 0,
                             "unsupported action diagnostic should preserve route")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(diag->action, "workspace.unknown") == 0,
                             "unsupported action diagnostic should preserve action")) {
        return 0;
    }
    if (!datalab_test_assert(diag->result_code == result.code,
                             "unsupported action diagnostic should preserve result code")) {
        return 0;
    }

    return 1;
}

int main(void) {
    if (!test_recipe_runtime_isolation_contract()) {
        return 1;
    }
    if (!test_authoring_consumes_session_keys()) {
        return 1;
    }
    if (!test_authoring_tab_apply_cancel_flow()) {
        return 1;
    }
    if (!test_authoring_popup_escape_closes_popup_only()) {
        return 1;
    }
    if (!test_authoring_takeover_snapshot_helpers()) {
        return 1;
    }
    if (!test_authoring_session_failed_safe_recovery_and_shutdown()) {
        return 1;
    }
    if (!test_fixed_visualizer_projection_drafts_and_restores()) {
        return 1;
    }
    if (!test_authoring_disables_session_mouse_controls()) {
        return 1;
    }
    if (!test_authoring_runtime_mutation_gate_covers_direct_paths()) {
        return 1;
    }
    if (!test_runtime_theme_cycle_keys()) {
        return 1;
    }
    if (!test_authoring_route_diagnostics_are_normalized()) {
        return 1;
    }
    puts("datalab authoring input contract test passed");
    return 0;
}
