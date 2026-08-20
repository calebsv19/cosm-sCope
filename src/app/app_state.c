#include "app/app_state.h"

#include "app/datalab_runtime_prefs.h"

#include <stdio.h>
#include <string.h>

int datalab_text_zoom_step_clamp(int step) {
    if (step < DATALAB_TEXT_ZOOM_STEP_MIN) {
        return DATALAB_TEXT_ZOOM_STEP_MIN;
    }
    if (step > DATALAB_TEXT_ZOOM_STEP_MAX) {
        return DATALAB_TEXT_ZOOM_STEP_MAX;
    }
    return step;
}

float datalab_text_zoom_step_multiplier(int step) {
    float multiplier;
    step = datalab_text_zoom_step_clamp(step);
    multiplier = 1.0f + ((float)step * 0.15f);
    if (multiplier < 0.55f) {
        multiplier = 0.55f;
    }
    if (multiplier > 2.0f) {
        multiplier = 2.0f;
    }
    return multiplier;
}

int datalab_playback_speed_index_clamp(int speed_index) {
    if (speed_index < DATALAB_PLAYBACK_SPEED_INDEX_MIN) {
        return DATALAB_PLAYBACK_SPEED_INDEX_MIN;
    }
    if (speed_index > DATALAB_PLAYBACK_SPEED_INDEX_MAX) {
        return DATALAB_PLAYBACK_SPEED_INDEX_MAX;
    }
    return speed_index;
}

uint32_t datalab_playback_interval_for_speed_index(int speed_index) {
    static const uint32_t k_intervals_ms[] = {480u, 240u, 120u, 60u, 30u};
    speed_index = datalab_playback_speed_index_clamp(speed_index);
    return k_intervals_ms[speed_index];
}

void datalab_panel_request_step(DatalabAppState *state, int delta, int open_selected) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->panel_selection_delta += delta;
    if (open_selected) {
        state->panel_open_selected_requested = 1;
    }
}

void datalab_panel_request_home(DatalabAppState *state, int open_selected) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) return;
    state->panel_selection_home_requested = 1;
    state->panel_selection_end_requested = 0;
    if (open_selected) state->panel_open_selected_requested = 1;
}

void datalab_panel_request_end(DatalabAppState *state, int open_selected) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) return;
    state->panel_selection_end_requested = 1;
    state->panel_selection_home_requested = 0;
    if (open_selected) state->panel_open_selected_requested = 1;
}

void datalab_panel_request_open_selected(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->panel_open_selected_requested = 1;
}

void datalab_panel_clear_request(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->panel_selection_delta = 0;
    state->panel_selection_home_requested = 0;
    state->panel_selection_end_requested = 0;
    state->panel_open_selected_requested = 0;
    state->panel_requested_pack_path[0] = '\0';
}

void datalab_panel_request_pack_path(DatalabAppState *state, const char *path) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->panel_requested_pack_path[0] = '\0';
    if (!path || path[0] == '\0') {
        return;
    }
    (void)snprintf(state->panel_requested_pack_path,
                   sizeof(state->panel_requested_pack_path),
                   "%s",
                   path);
}

int datalab_input_root_join_child_file(const char *root,
                                       const char *file_name,
                                       char *out_path,
                                       size_t out_cap) {
    int written = 0;
    if (out_path && out_cap > 0u) {
        out_path[0] = '\0';
    }
    if (!root || root[0] == '\0' || !file_name || file_name[0] == '\0' ||
        !out_path || out_cap == 0u) {
        return 0;
    }
    if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0 ||
        strchr(file_name, '/') || strchr(file_name, '\\')) {
        return 0;
    }
    written = snprintf(out_path, out_cap, "%s/%s", root, file_name);
    if (written < 0 || (size_t)written >= out_cap) {
        out_path[0] = '\0';
        return 0;
    }
    return 1;
}

int datalab_panel_request_pack_under_root(DatalabAppState *state,
                                          const char *root,
                                          const char *file_name) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return 0;
    }
    state->panel_requested_pack_path[0] = '\0';
    if (!root || root[0] == '\0' || !file_name || file_name[0] == '\0') {
        return 0;
    }
    return datalab_input_root_join_child_file(root,
                                             file_name,
                                             state->panel_requested_pack_path,
                                             sizeof(state->panel_requested_pack_path));
}

int datalab_panel_consume_requested_pack_path(DatalabAppState *state, char *out_path, size_t out_cap) {
    if (!state || !out_path || out_cap == 0u || state->panel_requested_pack_path[0] == '\0') {
        return 0;
    }
    (void)snprintf(out_path, out_cap, "%s", state->panel_requested_pack_path);
    state->panel_requested_pack_path[0] = '\0';
    return out_path[0] != '\0';
}

void datalab_panel_reset_interaction_state(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->panel_rescan_requested = 0;
    state->panel_selected_index = 0u;
    datalab_panel_clear_request(state);
}

void datalab_playback_toggle_active(DatalabAppState *state,
                                    uint32_t now_ticks,
                                    uint32_t fallback_interval_ms) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->playback_active = !state->playback_active;
    if (state->playback_interval_ms == 0u) {
        state->playback_interval_ms = fallback_interval_ms
            ? fallback_interval_ms
            : DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT;
    }
    if (state->playback_active) {
        state->playback_last_advance_ticks = now_ticks;
    }
}

void datalab_playback_set_speed_index(DatalabAppState *state,
                                      int speed_index,
                                      uint32_t now_ticks) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->playback_speed_index = datalab_playback_speed_index_clamp(speed_index);
    state->playback_interval_ms =
        datalab_playback_interval_for_speed_index(state->playback_speed_index);
    state->playback_last_advance_ticks = now_ticks;
}

void datalab_playback_set_mode(DatalabAppState *state, DatalabPlaybackMode mode) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    switch (mode) {
        case DATALAB_PLAYBACK_MODE_BOUNCE:
            state->playback_mode = DATALAB_PLAYBACK_MODE_BOUNCE;
            break;
        case DATALAB_PLAYBACK_MODE_LOOP:
        default:
            state->playback_mode = DATALAB_PLAYBACK_MODE_LOOP;
            break;
    }
    state->playback_direction = 1;
}

void datalab_playback_stop(DatalabAppState *state) {
    if (!state) {
        return;
    }
    state->playback_active = 0;
}

int datalab_app_state_select_input_root(DatalabAppState *state, const char *path) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || !path || path[0] == '\0') {
        return 0;
    }
    if (!datalab_input_root_select_recent(state->input_root,
                                          sizeof(state->input_root),
                                          state->recent_input_roots,
                                          &state->recent_input_root_count,
                                          DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                          path)) {
        return 0;
    }
    state->recent_input_root_dropdown_open = 0;
    datalab_panel_reset_interaction_state(state);
    datalab_playback_stop(state);
    return 1;
}

void datalab_app_state_request_picker(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->recent_input_root_dropdown_open = 0;
    state->open_picker_requested = 1;
}

void datalab_app_state_request_panel_rescan(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->panel_rescan_requested = 1;
}

void datalab_app_state_reset_interactions(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    state->vector_stride = 8;
    state->vector_scale = 0.15f;
    state->view_mode = (state->profile == DATALAB_PROFILE_DAW) ? DATALAB_VIEW_SPEED : DATALAB_VIEW_DENSITY;
    state->trace_cursor_index = 0u;
    state->trace_zoom_stub = 1.0f;
    state->trace_selection_stub_active = 0;
    state->trace_lane_visibility_index = 0u;
    state->trace_lane_cycle_requested = 0;
    datalab_panel_reset_interaction_state(state);
    state->recent_input_root_dropdown_open = 0;
    datalab_playback_stop(state);
    datalab_playback_set_mode(state, DATALAB_PLAYBACK_MODE_LOOP);
    state->playback_speed_index = DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT;
    state->playback_interval_ms =
        datalab_playback_interval_for_speed_index(state->playback_speed_index);
    state->playback_last_advance_ticks = 0u;
    datalab_raster_viewport_request_reset(&state->raster_viewport);
}

void datalab_profile_select_view_slot(DatalabAppState *state, int slot) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state)) {
        return;
    }
    if (state->profile == DATALAB_PROFILE_DAW) {
        switch (slot) {
            case 1:
                state->view_mode = DATALAB_VIEW_SPEED;
                break;
            case 2:
                state->view_mode = DATALAB_VIEW_DENSITY_VECTOR;
                break;
            case 3:
                state->view_mode = DATALAB_VIEW_DENSITY;
                break;
            default:
                break;
        }
        return;
    }
    switch (slot) {
        case 1:
            state->view_mode = DATALAB_VIEW_DENSITY;
            break;
        case 2:
            state->view_mode = DATALAB_VIEW_SPEED;
            break;
        case 3:
            state->view_mode = DATALAB_VIEW_DENSITY_VECTOR;
            break;
        default:
            break;
    }
}

void datalab_physics_adjust_vector_stride(DatalabAppState *state, int delta) {
    int next_stride = 0;
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) ||
        (state->profile != DATALAB_PROFILE_PHYSICS && state->profile != DATALAB_PROFILE_VOLUME) || delta == 0) {
        return;
    }
    next_stride = (int)state->vector_stride + delta;
    if (next_stride < 1) {
        next_stride = 1;
    }
    if (next_stride > 64) {
        next_stride = 64;
    }
    state->vector_stride = (uint32_t)next_stride;
}

void datalab_trace_step_cursor(DatalabAppState *state, int delta) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE || delta == 0) {
        return;
    }
    if (delta < 0) {
        if (state->trace_cursor_index > 0u) {
            state->trace_cursor_index--;
        }
        return;
    }
    state->trace_cursor_index++;
}

void datalab_trace_set_cursor_home(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE) {
        return;
    }
    state->trace_cursor_index = 0u;
}

void datalab_trace_set_cursor_end(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE) {
        return;
    }
    state->trace_cursor_index = (size_t)-1;
}

void datalab_trace_cycle_zoom(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE) {
        return;
    }
    state->trace_zoom_stub += 0.25f;
    if (state->trace_zoom_stub > 4.0f) {
        state->trace_zoom_stub = 1.0f;
    }
}

void datalab_trace_toggle_selection(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE) {
        return;
    }
    state->trace_selection_stub_active = !state->trace_selection_stub_active;
}

void datalab_trace_request_lane_cycle(DatalabAppState *state) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE) {
        return;
    }
    state->trace_lane_cycle_requested = 1;
}

void datalab_trace_clamp_cursor_to_count(DatalabAppState *state, size_t time_count) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE || time_count == 0u) {
        return;
    }
    if (state->trace_cursor_index == (size_t)-1 || state->trace_cursor_index >= time_count) {
        state->trace_cursor_index = time_count - 1u;
    }
}

void datalab_trace_apply_lane_cycle(DatalabAppState *state, size_t lane_count) {
    size_t cycle_span = 0u;
    if (!datalab_workspace_authoring_runtime_mutation_allowed(state) || state->profile != DATALAB_PROFILE_TRACE) {
        return;
    }
    if (lane_count == 0u) {
        lane_count = 1u;
    }
    if (state->trace_lane_cycle_requested) {
        cycle_span = lane_count + 1u;
        state->trace_lane_visibility_index =
            (state->trace_lane_visibility_index + 1u) % cycle_span;
        state->trace_lane_cycle_requested = 0;
    } else if (state->trace_lane_visibility_index > lane_count) {
        state->trace_lane_visibility_index = 0u;
    }
}

uint8_t datalab_workspace_authoring_theme_preset_clamp(int value) {
    if (value < (int)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT) {
        return (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT;
    }
    if (value > (int)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        return (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    }
    return (uint8_t)value;
}

int datalab_workspace_authoring_custom_theme_slot_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return DATALAB_CUSTOM_THEME_SLOT_COUNT - 1;
    }
    return value;
}

int datalab_workspace_authoring_custom_theme_token_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= 9) {
        return 8;
    }
    return value;
}

int datalab_workspace_authoring_custom_theme_channel_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= 3) {
        return 2;
    }
    return value;
}

uint8_t datalab_workspace_authoring_cycle_runtime_theme_preset(uint8_t current, int direction) {
    int preset = (int)current;
    const int first = (int)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT;
    const int last = (int)DATALAB_WORKSPACE_AUTHORING_THEME_GREYSCALE;
    const int count = (last - first) + 1;
    if (count <= 0) {
        return (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    }
    if (preset < first || preset > last) {
        preset = (int)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    }
    preset += (direction < 0) ? -1 : 1;
    while (preset < first) {
        preset += count;
    }
    while (preset > last) {
        preset -= count;
    }
    return (uint8_t)preset;
}

void datalab_workspace_authoring_sync_custom_theme_from_active_slot(DatalabAppState *state) {
    int slot_index = 0;
    if (!state) {
        return;
    }
    slot_index = datalab_workspace_authoring_custom_theme_slot_clamp(
        (int)state->workspace_authoring_custom_theme_active_slot);
    state->workspace_authoring_custom_theme_active_slot = (uint8_t)slot_index;
    state->workspace_authoring_custom_theme = state->workspace_authoring_custom_theme_slots[slot_index];
}

void datalab_workspace_authoring_capture_entry_snapshot(DatalabAppState *state) {
    int slot_index = 0;
    int selected_token = 0;
    int selected_channel = 0;
    if (!state) {
        return;
    }
    slot_index = datalab_workspace_authoring_custom_theme_slot_clamp(
        (int)state->workspace_authoring_custom_theme_active_slot);
    selected_token = datalab_workspace_authoring_custom_theme_token_clamp(
        (int)state->workspace_authoring_custom_theme_selected_token);
    selected_channel = datalab_workspace_authoring_custom_theme_channel_clamp(
        (int)state->workspace_authoring_custom_theme_selected_channel);
    state->workspace_authoring_custom_theme_active_slot = (uint8_t)slot_index;
    state->workspace_authoring_custom_theme_selected_token = (uint8_t)selected_token;
    state->workspace_authoring_custom_theme_selected_channel = (uint8_t)selected_channel;
    datalab_workspace_authoring_sync_custom_theme_from_active_slot(state);
    state->workspace_authoring_entry_text_zoom_step = state->text_zoom_step;
    state->workspace_authoring_entry_theme_preset_id =
        datalab_workspace_authoring_theme_preset_clamp((int)state->workspace_authoring_theme_preset_id);
    state->workspace_authoring_entry_custom_theme = state->workspace_authoring_custom_theme;
    state->workspace_authoring_entry_custom_theme_active_slot = (uint8_t)slot_index;
    memcpy(state->workspace_authoring_entry_custom_theme_slots,
           state->workspace_authoring_custom_theme_slots,
           sizeof(state->workspace_authoring_entry_custom_theme_slots));
    memcpy(state->workspace_authoring_entry_custom_theme_slot_names,
           state->workspace_authoring_custom_theme_slot_names,
           sizeof(state->workspace_authoring_entry_custom_theme_slot_names));
    state->workspace_authoring_entry_custom_theme_selected_token = (uint8_t)selected_token;
    state->workspace_authoring_entry_custom_theme_selected_channel = (uint8_t)selected_channel;
    datalab_workspace_authoring_projection_capture_entry(state);
}

void datalab_workspace_authoring_draft_begin(DatalabAppState *state) {
    if (!state) {
        return;
    }
    state->workspace_authoring_stub_active = 1;
    state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state->workspace_authoring_pending_stub = 0u;
    state->workspace_authoring_custom_theme_popup_open = 0u;
    datalab_workspace_authoring_capture_entry_snapshot(state);
    state->workspace_authoring_entry_count += 1u;
    state->workspace_authoring_entry_chord_mask = 0u;
}

void datalab_workspace_authoring_cycle_overlay(DatalabAppState *state) {
    if (!state) {
        return;
    }
    if (state->workspace_authoring_overlay_mode == DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE) {
        state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME;
    } else {
        state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    }
    state->workspace_authoring_pending_stub = 1u;
    state->workspace_authoring_overlay_cycle_count += 1u;
}

void datalab_workspace_authoring_draft_apply(DatalabAppState *state) {
    if (!state) {
        return;
    }
    state->workspace_authoring_pending_stub = 0u;
    state->workspace_authoring_custom_theme_popup_open = 0u;
    datalab_workspace_authoring_capture_entry_snapshot(state);
    state->workspace_authoring_apply_count += 1u;
}

int datalab_workspace_authoring_draft_cancel(DatalabAppState *state) {
    int restored_text_zoom = 0;
    if (!state) {
        return 0;
    }
    const int restored_step =
        datalab_text_zoom_step_clamp(state->workspace_authoring_entry_text_zoom_step);
    restored_text_zoom = (state->text_zoom_step != restored_step) ? 1 : 0;
    state->text_zoom_step = restored_step;
    state->workspace_authoring_theme_preset_id =
        datalab_workspace_authoring_theme_preset_clamp(
            (int)state->workspace_authoring_entry_theme_preset_id);
    state->workspace_authoring_custom_theme_active_slot =
        (uint8_t)datalab_workspace_authoring_custom_theme_slot_clamp(
            (int)state->workspace_authoring_entry_custom_theme_active_slot);
    memcpy(state->workspace_authoring_custom_theme_slots,
           state->workspace_authoring_entry_custom_theme_slots,
           sizeof(state->workspace_authoring_custom_theme_slots));
    memcpy(state->workspace_authoring_custom_theme_slot_names,
           state->workspace_authoring_entry_custom_theme_slot_names,
           sizeof(state->workspace_authoring_custom_theme_slot_names));
    state->workspace_authoring_custom_theme = state->workspace_authoring_entry_custom_theme;
    datalab_workspace_authoring_sync_custom_theme_from_active_slot(state);
    datalab_workspace_authoring_projection_restore_entry(state);
    state->workspace_authoring_custom_theme_selected_token =
        (uint8_t)datalab_workspace_authoring_custom_theme_token_clamp(
            (int)state->workspace_authoring_entry_custom_theme_selected_token);
    state->workspace_authoring_custom_theme_selected_channel =
        (uint8_t)datalab_workspace_authoring_custom_theme_channel_clamp(
            (int)state->workspace_authoring_entry_custom_theme_selected_channel);
    state->workspace_authoring_pending_stub = 0u;
    state->workspace_authoring_stub_active = 0;
    state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state->workspace_authoring_custom_theme_popup_open = 0u;
    state->workspace_authoring_entry_chord_mask = 0u;
    state->workspace_authoring_cancel_count += 1u;
    return restored_text_zoom;
}

int datalab_workspace_authoring_close_custom_theme_popup(DatalabAppState *state) {
    if (!state || !state->workspace_authoring_custom_theme_popup_open) {
        return 0;
    }
    state->workspace_authoring_custom_theme_popup_open = 0u;
    return 1;
}

void datalab_raster_viewport_state_init(DatalabRasterViewportState *state) {
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));
    (void)core_viewport2d_init(&state->viewport);
    state->fit_mode = 1;
    state->reset_requested = 1;
}

void datalab_raster_viewport_request_reset(DatalabRasterViewportState *state) {
    if (!state) {
        return;
    }
    state->fit_mode = 1;
    state->reset_requested = 1;
    state->drag_active = 0;
}

static void datalab_raster_viewport_apply_fit(DatalabRasterViewportState *state,
                                              int view_width,
                                              int view_height,
                                              uint32_t content_width,
                                              uint32_t content_height) {
    CoreResult reset_r;
    if (!state || view_width <= 0 || view_height <= 0 || content_width == 0u || content_height == 0u) {
        return;
    }
    state->viewport.min_zoom = 0.0001f;
    state->viewport.max_zoom = 64.0f;
    reset_r = core_viewport2d_reset_to_fit(&state->viewport,
                                           (float)view_width,
                                           (float)view_height,
                                           (float)content_width,
                                           (float)content_height);
    if (reset_r.code != CORE_OK) {
        state->valid = 0;
        return;
    }
    state->valid = 1;
    state->fit_mode = 1;
    state->reset_requested = 0;
    state->drag_active = 0;
    state->view_width = view_width;
    state->view_height = view_height;
    state->content_width = content_width;
    state->content_height = content_height;
}

void datalab_raster_viewport_sync_state(DatalabRasterViewportState *state,
                                        int view_width,
                                        int view_height,
                                        uint32_t content_width,
                                        uint32_t content_height) {
    if (!state || view_width <= 0 || view_height <= 0 || content_width == 0u || content_height == 0u) {
        return;
    }
    if (!state->valid) {
        state->fit_mode = 1;
        state->reset_requested = 1;
    }
    if (state->content_width != content_width || state->content_height != content_height) {
        state->content_width = content_width;
        state->content_height = content_height;
        state->fit_mode = 1;
        state->reset_requested = 1;
    }
    if (state->fit_mode && (state->view_width != view_width || state->view_height != view_height)) {
        state->reset_requested = 1;
    }
    if (state->reset_requested) {
        datalab_raster_viewport_apply_fit(state, view_width, view_height, content_width, content_height);
        return;
    }
    state->view_width = view_width;
    state->view_height = view_height;
}

int datalab_raster_viewport_zoom_at_screen_anchor(DatalabRasterViewportState *state,
                                                  int screen_x,
                                                  int screen_y,
                                                  float zoom_factor) {
    if (!state || !state->valid || zoom_factor <= 0.0f) {
        return 0;
    }
    if (core_viewport2d_zoom_at_screen_anchor(&state->viewport,
                                              (float)screen_x,
                                              (float)screen_y,
                                              zoom_factor)
        .code != CORE_OK) {
        return 0;
    }
    state->fit_mode = 0;
    state->reset_requested = 0;
    state->valid = 1;
    state->drag_active = 0;
    return 1;
}

int datalab_raster_viewport_begin_drag(DatalabRasterViewportState *state, int screen_x, int screen_y) {
    if (!state || !state->valid) {
        return 0;
    }
    state->drag_active = 1;
    state->last_mouse_x = screen_x;
    state->last_mouse_y = screen_y;
    return 1;
}

void datalab_raster_viewport_end_drag(DatalabRasterViewportState *state) {
    if (!state) {
        return;
    }
    state->drag_active = 0;
}

int datalab_raster_viewport_drag_to(DatalabRasterViewportState *state, int screen_x, int screen_y) {
    if (!state || !state->drag_active || !state->valid) {
        return 0;
    }
    if (core_viewport2d_pan_by(&state->viewport,
                               (float)(screen_x - state->last_mouse_x),
                               (float)(screen_y - state->last_mouse_y))
        .code != CORE_OK) {
        state->drag_active = 0;
        return 0;
    }
    state->fit_mode = 0;
    state->reset_requested = 0;
    state->valid = 1;
    state->last_mouse_x = screen_x;
    state->last_mouse_y = screen_y;
    return 1;
}

void datalab_raster_viewport_copy_for_runtime(DatalabRasterViewportState *dst,
                                              const DatalabRasterViewportState *src) {
    if (!dst || !src) {
        return;
    }
    *dst = *src;
    dst->drag_active = 0;
}

void datalab_raster_viewport_toggle_actual_pixel(DatalabAppState *state) {
    DatalabRasterViewportState *viewport = NULL;
    if (!state || !datalab_profile_supports_raster_viewport(state->profile)) return;
    viewport = &state->raster_viewport;
    state->raster_actual_pixel_mode = !state->raster_actual_pixel_mode;
    if (state->raster_actual_pixel_mode) {
        viewport->viewport.zoom = 1.0f;
        viewport->fit_mode = 0;
        viewport->reset_requested = 0;
        viewport->valid = 1;
    } else {
        datalab_raster_viewport_request_reset(viewport);
    }
}

void datalab_raster_probe_at_screen(DatalabAppState *state, int screen_x, int screen_y) {
    DatalabRasterViewportState *viewport = NULL;
    float zoom = 0.0f;
    float source_x = 0.0f;
    float source_y = 0.0f;
    if (!state || !datalab_profile_supports_raster_viewport(state->profile)) return;
    viewport = &state->raster_viewport;
    zoom = viewport->viewport.zoom;
    if (!viewport->valid || zoom <= 0.0f || screen_x < (int)viewport->viewport.pan_x || screen_y < (int)viewport->viewport.pan_y) {
        state->raster_probe_valid = 0;
        return;
    }
    source_x = ((float)screen_x - viewport->viewport.pan_x) / zoom;
    source_y = ((float)screen_y - viewport->viewport.pan_y) / zoom;
    if (source_x < 0.0f || source_y < 0.0f || source_x >= (float)viewport->content_width || source_y >= (float)viewport->content_height) {
        state->raster_probe_valid = 0;
        return;
    }
    state->raster_probe_x = (uint32_t)source_x;
    state->raster_probe_y = (uint32_t)source_y;
    state->raster_probe_valid = 1;
}

void datalab_sampling_mode_cycle(DatalabAppState *state) {
    if (!state || !datalab_profile_supports_raster_viewport(state->profile)) return;
    state->sampling_mode = state->sampling_mode == DATALAB_SAMPLING_MODE_NEAREST
                               ? DATALAB_SAMPLING_MODE_LINEAR
                               : DATALAB_SAMPLING_MODE_NEAREST;
}

int datalab_profile_supports_raster_viewport(DatalabProfile profile) {
    return profile == DATALAB_PROFILE_SKETCH || profile == DATALAB_PROFILE_IMAGE;
}

void datalab_app_state_init(DatalabAppState *state, const char *pack_path, DatalabProfile profile) {
    DatalabWorkspaceCustomTheme default_theme;
    int i;
    if (!state) return;
    state->pack_path = pack_path;
    state->profile = profile;
    state->input_catalog = NULL;
    state->runtime_owner = NULL;
    state->async_decode = NULL;
    state->async_decode_frame_ready = 0;
    state->input_root[0] = '\0';
    state->recent_input_root_count = 0u;
    state->recent_input_root_dropdown_open = 0;
    state->view_mode = (profile == DATALAB_PROFILE_DAW) ? DATALAB_VIEW_SPEED : DATALAB_VIEW_DENSITY;
    state->text_zoom_step = 0;
    state->vector_stride = 8;
    state->vector_scale = 0.15f;
    state->trace_cursor_index = 0u;
    state->trace_zoom_stub = 1.0f;
    state->trace_selection_stub_active = 0;
    state->trace_lane_visibility_index = 0u;
    state->trace_lane_cycle_requested = 0;
    state->open_picker_requested = 0;
    state->panel_rescan_requested = 0;
    state->panel_selection_delta = 0;
    state->panel_selection_home_requested = 0;
    state->panel_selection_end_requested = 0;
    state->panel_selected_index = 0u;
    state->panel_open_selected_requested = 0;
    state->panel_requested_pack_path[0] = '\0';
    state->playback_active = 0;
    state->playback_mode = DATALAB_PLAYBACK_MODE_LOOP;
    state->playback_direction = 1;
    state->playback_speed_index = DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT;
    state->playback_interval_ms = DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT;
    state->playback_last_advance_ticks = 0u;
    state->session_hud_collapsed = 0;
    state->sampling_mode = DATALAB_SAMPLING_MODE_DEFAULT;
    state->raster_actual_pixel_mode = 0;
    state->raster_alpha_checkerboard = 0;
    state->raster_probe_valid = 0;
    state->raster_probe_x = 0u;
    state->raster_probe_y = 0u;
    datalab_raster_viewport_state_init(&state->raster_viewport);
    state->workspace_authoring_stub_active = 0;
    state->workspace_authoring_entry_chord_mask = 0u;
    state->workspace_authoring_entry_count = 0u;
    state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state->workspace_authoring_pending_stub = 0u;
    default_theme = (DatalabWorkspaceCustomTheme){
        12, 14, 20,
        54, 36, 74,
        24, 28, 38,
        112, 124, 146,
        226, 234, 246,
        178, 194, 220,
        34, 40, 58,
        48, 58, 84,
        116, 136, 184
    };
    state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    state->workspace_authoring_custom_theme = default_theme;
    state->workspace_authoring_custom_theme_active_slot = 0u;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        state->workspace_authoring_custom_theme_slots[i] = default_theme;
        (void)snprintf(state->workspace_authoring_custom_theme_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "custom_%d",
                       i + 1);
    }
    state->workspace_authoring_custom_theme = state->workspace_authoring_custom_theme_slots[0];
    state->workspace_authoring_custom_theme_popup_open = 0u;
    state->workspace_authoring_custom_theme_selected_token = 0u;
    state->workspace_authoring_custom_theme_selected_channel = 0u;
    datalab_workspace_authoring_projection_init(&state->workspace_authoring_projection);
    datalab_workspace_authoring_capture_entry_snapshot(state);
    state->workspace_authoring_overlay_cycle_count = 0u;
    state->workspace_authoring_apply_count = 0u;
    state->workspace_authoring_cancel_count = 0u;
    datalab_workspace_authoring_session_init(state);
}

const char *datalab_view_mode_name(DatalabViewMode mode) {
    switch (mode) {
        case DATALAB_VIEW_DENSITY: return "density";
        case DATALAB_VIEW_SPEED: return "speed";
        case DATALAB_VIEW_DENSITY_VECTOR: return "density+vector";
        default: return "unknown";
    }
}

const char *datalab_workspace_authoring_overlay_mode_name(DatalabWorkspaceAuthoringOverlayMode mode) {
    switch (mode) {
        case DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE: return "pane";
        case DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME: return "font/theme";
        default: return "unknown";
    }
}
