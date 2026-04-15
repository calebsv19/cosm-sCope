#include "app/app_state.h"

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
}

const char *datalab_view_mode_name(DatalabViewMode mode) {
    switch (mode) {
        case DATALAB_VIEW_DENSITY: return "density";
        case DATALAB_VIEW_SPEED: return "speed";
        case DATALAB_VIEW_DENSITY_VECTOR: return "density+vector";
        default: return "unknown";
    }
}
