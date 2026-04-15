#ifndef DATALAB_APP_STATE_H
#define DATALAB_APP_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "data/pack_loader.h"

#define DATALAB_APP_PATH_CAP 1024

typedef enum DatalabViewMode {
    DATALAB_VIEW_DENSITY = 1,
    DATALAB_VIEW_SPEED = 2,
    DATALAB_VIEW_DENSITY_VECTOR = 3
} DatalabViewMode;

#define DATALAB_TEXT_ZOOM_STEP_MIN (-4)
#define DATALAB_TEXT_ZOOM_STEP_MAX 5

typedef struct DatalabAppState {
    const char *pack_path;
    DatalabProfile profile;
    char input_root[DATALAB_APP_PATH_CAP];
    DatalabViewMode view_mode;
    int text_zoom_step;
    uint32_t vector_stride;
    float vector_scale;
    size_t trace_cursor_index;
    float trace_zoom_stub;
    int trace_selection_stub_active;
    size_t trace_lane_visibility_index; /* 0 = all, 1..N = one lane by index */
    int trace_lane_cycle_requested;
    int open_picker_requested;
    int panel_rescan_requested;
    int panel_selection_delta;
    size_t panel_selected_index;
    int panel_open_selected_requested;
    char panel_requested_pack_path[DATALAB_APP_PATH_CAP];
    int workspace_authoring_stub_active;
    uint8_t workspace_authoring_entry_chord_mask;
    uint32_t workspace_authoring_entry_count;
} DatalabAppState;

void datalab_app_state_init(DatalabAppState *state, const char *pack_path, DatalabProfile profile);
const char *datalab_view_mode_name(DatalabViewMode mode);
int datalab_text_zoom_step_clamp(int step);
float datalab_text_zoom_step_multiplier(int step);

#endif
