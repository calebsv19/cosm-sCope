#include <stdio.h>
#include <string.h>

#include "app/app_state.h"
#include "render/render_view_internal.h"

static int datalab_test_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "panel-policy-contract: %s\n", message ? message : "assertion failed");
        return 0;
    }
    return 1;
}

static void datalab_test_cache_set_file(DatalabPackPanelCache *cache, size_t index, const char *name) {
    if (!cache || index >= DATALAB_PANEL_MAX_FILES || !name) {
        return;
    }
    snprintf(cache->files[index], sizeof(cache->files[index]), "%s", name);
}

static int test_empty_root_resets_panel_state(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 2u;
    cache.last_scan_ticks = 99u;
    state.panel_selected_index = 1u;
    state.panel_selection_delta = 3;
    state.panel_open_selected_requested = 1;
    state.playback_active = 1;
    snprintf(state.panel_requested_pack_path, sizeof(state.panel_requested_pack_path), "stale.pack");

    datalab_panel_apply_state(&state, &cache, "", 0, 0u);
    if (!datalab_test_assert(cache.file_count == 0u, "empty root should clear cached files")) {
        return 0;
    }
    if (!datalab_test_assert(cache.last_scan_ticks == 0u, "empty root should clear scan timestamp")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_selected_index == 0u && state.panel_selection_delta == 0,
                             "empty root should reset panel selection state")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_open_selected_requested == 0,
                             "empty root should clear open-selected request")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_requested_pack_path[0] == '\0',
                             "empty root should clear requested pack path")) {
        return 0;
    }
    if (!datalab_test_assert(state.playback_active == 0, "empty root should stop playback")) {
        return 0;
    }
    return 1;
}

static int test_rescan_aligns_selection_to_active_file(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "/tmp/root/beta.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 3u;
    datalab_test_cache_set_file(&cache, 0u, "alpha.pack");
    datalab_test_cache_set_file(&cache, 1u, "beta.pack");
    datalab_test_cache_set_file(&cache, 2u, "gamma.pack");
    state.panel_selected_index = 0u;

    datalab_panel_apply_state(&state, &cache, "/tmp/root", 1, 50u);
    if (!datalab_test_assert(state.panel_selected_index == 1u,
                             "rescan should realign panel selection to the active file")) {
        return 0;
    }
    return 1;
}

static int test_selection_delta_and_open_request_emit_requested_path(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 2u;
    datalab_test_cache_set_file(&cache, 0u, "alpha.pack");
    datalab_test_cache_set_file(&cache, 1u, "beta.pack");
    state.panel_selected_index = 0u;
    state.panel_selection_delta = 5;
    state.panel_open_selected_requested = 1;

    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 60u);
    if (!datalab_test_assert(state.panel_selected_index == 1u,
                             "selection delta should clamp to the last file")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_selection_delta == 0,
                             "selection delta should clear after being applied")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_open_selected_requested == 0,
                             "open-selected request should clear after path emission")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/beta.pack") == 0,
                             "open-selected request should emit the selected pack path")) {
        return 0;
    }
    return 1;
}

static int test_manual_selection_delta_wraps_at_edges(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 3u;
    datalab_test_cache_set_file(&cache, 0u, "alpha.pack");
    datalab_test_cache_set_file(&cache, 1u, "beta.pack");
    datalab_test_cache_set_file(&cache, 2u, "gamma.pack");

    state.panel_selected_index = 0u;
    state.panel_selection_delta = -1;
    state.panel_open_selected_requested = 1;
    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 60u);
    if (!datalab_test_assert(state.panel_selected_index == 2u,
                             "manual previous from the first file should wrap to the last file")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/gamma.pack") == 0,
                             "manual previous wrap should request the last path")) {
        return 0;
    }

    state.panel_requested_pack_path[0] = '\0';
    state.panel_selected_index = 2u;
    state.panel_selection_delta = 1;
    state.panel_open_selected_requested = 1;
    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 70u);
    if (!datalab_test_assert(state.panel_selected_index == 0u,
                             "manual next from the last file should wrap to the first file")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/alpha.pack") == 0,
                             "manual next wrap should request the first path")) {
        return 0;
    }

    return 1;
}

static int test_playback_advance_updates_selection_and_request(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 2u;
    datalab_test_cache_set_file(&cache, 0u, "alpha.pack");
    datalab_test_cache_set_file(&cache, 1u, "beta.pack");
    state.playback_active = 1;
    state.playback_interval_ms = 0u;
    state.playback_last_advance_ticks = 10u;
    state.panel_selected_index = 0u;

    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 200u);
    if (!datalab_test_assert(state.playback_interval_ms == DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT,
                             "playback should seed a default interval when unset")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_selected_index == 1u,
                             "playback advance should step to the next file")) {
        return 0;
    }
    if (!datalab_test_assert(state.playback_last_advance_ticks == 200u,
                             "playback advance should update the last advance tick")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/beta.pack") == 0,
                             "playback advance should emit a requested pack path")) {
        return 0;
    }
    return 1;
}

static int test_playback_loop_wraps_forward_and_reverse(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 3u;
    datalab_test_cache_set_file(&cache, 0u, "alpha.pack");
    datalab_test_cache_set_file(&cache, 1u, "beta.pack");
    datalab_test_cache_set_file(&cache, 2u, "gamma.pack");
    state.playback_active = 1;
    state.playback_mode = DATALAB_PLAYBACK_MODE_LOOP;
    state.playback_direction = 1;
    state.playback_interval_ms = 100u;
    state.playback_last_advance_ticks = 0u;
    state.panel_selected_index = 2u;

    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 100u);
    if (!datalab_test_assert(state.panel_selected_index == 0u,
                             "loop playback should wrap from last to first")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/alpha.pack") == 0,
                             "loop wrap should request the first path")) {
        return 0;
    }

    state.panel_requested_pack_path[0] = '\0';
    state.playback_direction = -1;
    state.playback_last_advance_ticks = 100u;
    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 200u);
    if (!datalab_test_assert(state.panel_selected_index == 2u,
                             "reverse loop playback should wrap from first to last")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/gamma.pack") == 0,
                             "reverse loop wrap should request the last path")) {
        return 0;
    }
    return 1;
}

static int test_playback_bounce_reverses_at_edges(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 3u;
    datalab_test_cache_set_file(&cache, 0u, "alpha.pack");
    datalab_test_cache_set_file(&cache, 1u, "beta.pack");
    datalab_test_cache_set_file(&cache, 2u, "gamma.pack");
    state.playback_active = 1;
    state.playback_mode = DATALAB_PLAYBACK_MODE_BOUNCE;
    state.playback_direction = 1;
    state.playback_interval_ms = 100u;
    state.playback_last_advance_ticks = 0u;
    state.panel_selected_index = 2u;

    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 100u);
    if (!datalab_test_assert(state.playback_direction == -1,
                             "bounce playback should reverse direction at the last file")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_selected_index == 1u,
                             "bounce playback should step inward from the last file")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/beta.pack") == 0,
                             "bounce reverse should request the inward path")) {
        return 0;
    }

    state.panel_requested_pack_path[0] = '\0';
    state.playback_last_advance_ticks = 100u;
    state.panel_selected_index = 0u;
    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 200u);
    if (!datalab_test_assert(state.playback_direction == 1,
                             "bounce playback should reverse direction at the first file")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_selected_index == 1u,
                             "bounce playback should step inward from the first file")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(state.panel_requested_pack_path, "/tmp/root/beta.pack") == 0,
                             "bounce start reverse should request the inward path")) {
        return 0;
    }
    return 1;
}

static int test_playback_speed_index_seeds_interval(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 2u;
    datalab_test_cache_set_file(&cache, 0u, "alpha.pack");
    datalab_test_cache_set_file(&cache, 1u, "beta.pack");
    state.playback_active = 1;
    state.playback_speed_index = 3;
    state.playback_interval_ms = 0u;
    state.playback_last_advance_ticks = 0u;
    state.panel_selected_index = 0u;

    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 60u);
    if (!datalab_test_assert(state.playback_interval_ms == 60u,
                             "playback speed index should seed the interval")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_selected_index == 1u,
                             "seeded fast interval should still advance when elapsed")) {
        return 0;
    }

    state.playback_speed_index = 99;
    state.playback_interval_ms = 0u;
    state.playback_last_advance_ticks = 60u;
    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 180u);
    if (!datalab_test_assert(state.playback_speed_index == DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT,
                             "invalid speed index should reset to the default index")) {
        return 0;
    }
    if (!datalab_test_assert(state.playback_interval_ms == DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT,
                             "invalid speed index should seed the default interval")) {
        return 0;
    }
    return 1;
}

static int test_requested_pack_path_helper_consumes_once(void) {
    DatalabAppState state;
    char consumed[DATALAB_APP_PATH_CAP];

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(consumed, 0, sizeof(consumed));

    datalab_panel_request_pack_under_root(&state, "/tmp/root", "alpha.pack");
    if (!datalab_test_assert(datalab_panel_consume_requested_pack_path(&state,
                                                                       consumed,
                                                                       sizeof(consumed)) == 1,
                             "requested pack helper should consume an emitted path")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(consumed, "/tmp/root/alpha.pack") == 0,
                             "requested pack helper should preserve the emitted path")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_requested_pack_path[0] == '\0',
                             "requested pack helper should clear app-state storage after consume")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_panel_consume_requested_pack_path(&state,
                                                                       consumed,
                                                                       sizeof(consumed)) == 0,
                             "requested pack helper should not consume the same path twice")) {
        return 0;
    }
    return 1;
}

static int test_input_root_child_join_rejects_non_child_names(void) {
    char joined[DATALAB_APP_PATH_CAP];
    char tiny[8];

    memset(joined, 0, sizeof(joined));
    if (!datalab_test_assert(datalab_input_root_join_child_file("/tmp/root",
                                                               "alpha.pack",
                                                               joined,
                                                               sizeof(joined)) == 1,
                             "child join should accept a direct file name")) {
        return 0;
    }
    if (!datalab_test_assert(strcmp(joined, "/tmp/root/alpha.pack") == 0,
                             "child join should produce a root-contained path")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_input_root_join_child_file("/tmp/root",
                                                               "../escape.pack",
                                                               joined,
                                                               sizeof(joined)) == 0,
                             "child join should reject parent traversal")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_input_root_join_child_file("/tmp/root",
                                                               "nested/escape.pack",
                                                               joined,
                                                               sizeof(joined)) == 0,
                             "child join should reject slash-separated names")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_input_root_join_child_file("/tmp/root",
                                                               "nested\\escape.pack",
                                                               joined,
                                                               sizeof(joined)) == 0,
                             "child join should reject backslash-separated names")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_input_root_join_child_file("/tmp/root",
                                                               "..",
                                                               joined,
                                                               sizeof(joined)) == 0,
                             "child join should reject dot-dot as a filename")) {
        return 0;
    }
    memset(tiny, 0, sizeof(tiny));
    if (!datalab_test_assert(datalab_input_root_join_child_file("/tmp/root",
                                                               "alpha.pack",
                                                               tiny,
                                                               sizeof(tiny)) == 0,
                             "child join should reject truncated output paths")) {
        return 0;
    }
    if (!datalab_test_assert(tiny[0] == '\0',
                             "child join should clear truncated output paths")) {
        return 0;
    }
    return 1;
}

static int test_panel_open_selected_rejects_non_child_cache_name(void) {
    DatalabAppState state;
    DatalabPackPanelCache cache;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&cache, 0, sizeof(cache));
    cache.file_count = 1u;
    datalab_test_cache_set_file(&cache, 0u, "../escape.pack");
    state.panel_selected_index = 0u;
    state.panel_open_selected_requested = 1;

    datalab_panel_apply_state(&state, &cache, "/tmp/root", 0, 60u);
    if (!datalab_test_assert(state.panel_open_selected_requested == 0,
                             "rejected non-child open request should still clear the request flag")) {
        return 0;
    }
    if (!datalab_test_assert(state.panel_requested_pack_path[0] == '\0',
                             "rejected non-child cache file should not emit a requested path")) {
        return 0;
    }
    if (!datalab_test_assert(strstr(cache.status, "outside input root") != NULL,
                             "rejected non-child cache file should leave an actionable panel status")) {
        return 0;
    }
    return 1;
}

static int test_supported_file_scan_status_is_actionable(void) {
    DatalabSupportedFileScanResult scan;
    char status[160];

    memset(&scan, 0, sizeof(scan));
    memset(status, 0, sizeof(status));
    scan.root_unavailable = 1;
    datalab_format_supported_file_scan_status(&scan,
                                              "/tmp/missing-root",
                                              "press O to reselect",
                                              status,
                                              sizeof(status));
    if (!datalab_test_assert(strstr(status, "input root unavailable: /tmp/missing-root") != NULL,
                             "root-unavailable status should include the unavailable root")) {
        return 0;
    }
    if (!datalab_test_assert(strstr(status, "press O to reselect") != NULL,
                             "root-unavailable status should include recovery action")) {
        return 0;
    }

    memset(&scan, 0, sizeof(scan));
    memset(status, 0, sizeof(status));
    scan.invalid_request = 1;
    datalab_format_supported_file_scan_status(&scan,
                                              "",
                                              "choose folder",
                                              status,
                                              sizeof(status));
    if (!datalab_test_assert(strstr(status, "invalid file scan request") != NULL,
                             "invalid-scan status should identify the request problem")) {
        return 0;
    }
    if (!datalab_test_assert(strstr(status, "root=(empty)") != NULL,
                             "invalid-scan status should include bounded root context")) {
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_empty_root_resets_panel_state()) {
        return 1;
    }
    if (!test_rescan_aligns_selection_to_active_file()) {
        return 1;
    }
    if (!test_selection_delta_and_open_request_emit_requested_path()) {
        return 1;
    }
    if (!test_manual_selection_delta_wraps_at_edges()) {
        return 1;
    }
    if (!test_playback_advance_updates_selection_and_request()) {
        return 1;
    }
    if (!test_playback_loop_wraps_forward_and_reverse()) {
        return 1;
    }
    if (!test_playback_bounce_reverses_at_edges()) {
        return 1;
    }
    if (!test_playback_speed_index_seeds_interval()) {
        return 1;
    }
    if (!test_requested_pack_path_helper_consumes_once()) {
        return 1;
    }
    if (!test_input_root_child_join_rejects_non_child_names()) {
        return 1;
    }
    if (!test_panel_open_selected_rejects_non_child_cache_name()) {
        return 1;
    }
    if (!test_supported_file_scan_status_is_actionable()) {
        return 1;
    }
    puts("datalab panel policy contract test passed");
    return 0;
}
