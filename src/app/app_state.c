#include "app/app_state.h"

void datalab_app_state_init(DatalabAppState *state, const char *pack_path, DatalabProfile profile) {
    if (!state) return;
    state->pack_path = pack_path;
    state->profile = profile;
    state->view_mode = (profile == DATALAB_PROFILE_DAW) ? DATALAB_VIEW_SPEED : DATALAB_VIEW_DENSITY;
    state->vector_stride = 8;
    state->vector_scale = 0.15f;
    state->trace_cursor_index = 0u;
    state->trace_zoom_stub = 1.0f;
    state->trace_selection_stub_active = 0;
    state->trace_lane_visibility_index = 0u;
    state->trace_lane_cycle_requested = 0;
}

const char *datalab_view_mode_name(DatalabViewMode mode) {
    switch (mode) {
        case DATALAB_VIEW_DENSITY: return "density";
        case DATALAB_VIEW_SPEED: return "speed";
        case DATALAB_VIEW_DENSITY_VECTOR: return "density+vector";
        default: return "unknown";
    }
}
