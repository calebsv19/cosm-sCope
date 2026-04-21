#include "app/app_state.h"

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

void datalab_app_state_init(DatalabAppState *state, const char *pack_path, DatalabProfile profile) {
    DatalabWorkspaceCustomTheme default_theme;
    int i;
    if (!state) return;
    state->pack_path = pack_path;
    state->profile = profile;
    state->input_root[0] = '\0';
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
    state->panel_selected_index = 0u;
    state->panel_open_selected_requested = 0;
    state->panel_requested_pack_path[0] = '\0';
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
    state->workspace_authoring_entry_text_zoom_step = state->text_zoom_step;
    state->workspace_authoring_entry_theme_preset_id = state->workspace_authoring_theme_preset_id;
    state->workspace_authoring_entry_custom_theme = state->workspace_authoring_custom_theme;
    state->workspace_authoring_entry_custom_theme_active_slot =
        state->workspace_authoring_custom_theme_active_slot;
    memcpy(state->workspace_authoring_entry_custom_theme_slots,
           state->workspace_authoring_custom_theme_slots,
           sizeof(state->workspace_authoring_entry_custom_theme_slots));
    memcpy(state->workspace_authoring_entry_custom_theme_slot_names,
           state->workspace_authoring_custom_theme_slot_names,
           sizeof(state->workspace_authoring_entry_custom_theme_slot_names));
    state->workspace_authoring_entry_custom_theme_selected_token =
        state->workspace_authoring_custom_theme_selected_token;
    state->workspace_authoring_entry_custom_theme_selected_channel =
        state->workspace_authoring_custom_theme_selected_channel;
    state->workspace_authoring_overlay_cycle_count = 0u;
    state->workspace_authoring_apply_count = 0u;
    state->workspace_authoring_cancel_count = 0u;
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
