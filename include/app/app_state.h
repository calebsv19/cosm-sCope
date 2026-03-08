#ifndef DATALAB_APP_STATE_H
#define DATALAB_APP_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "data/pack_loader.h"

typedef enum DatalabViewMode {
    DATALAB_VIEW_DENSITY = 1,
    DATALAB_VIEW_SPEED = 2,
    DATALAB_VIEW_DENSITY_VECTOR = 3
} DatalabViewMode;

typedef struct DatalabAppState {
    const char *pack_path;
    DatalabProfile profile;
    DatalabViewMode view_mode;
    uint32_t vector_stride;
    float vector_scale;
    size_t trace_cursor_index;
    float trace_zoom_stub;
    int trace_selection_stub_active;
    size_t trace_lane_visibility_index; /* 0 = all, 1..N = one lane by index */
    int trace_lane_cycle_requested;
} DatalabAppState;

void datalab_app_state_init(DatalabAppState *state, const char *pack_path, DatalabProfile profile);
const char *datalab_view_mode_name(DatalabViewMode mode);

#endif
