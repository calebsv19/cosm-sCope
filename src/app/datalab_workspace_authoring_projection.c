#include "app/app_state.h"

#include <string.h>

enum { DATALAB_WORKSPACE_PROJECTION_ROOT = 0, DATALAB_WORKSPACE_PROJECTION_PROFILE = 1,
       DATALAB_WORKSPACE_PROJECTION_SOURCE_CONTROLS = 2 };

void datalab_workspace_authoring_projection_init(DatalabWorkspaceAuthoringProjection *projection) {
    if (!projection) return;
    memset(projection, 0, sizeof(*projection));
    projection->profile_surface_ratio = 0.72f;
    projection->nodes[DATALAB_WORKSPACE_PROJECTION_ROOT] = (CorePaneNode){
        CORE_PANE_NODE_SPLIT, 0u, CORE_PANE_AXIS_HORIZONTAL, projection->profile_surface_ratio,
        DATALAB_WORKSPACE_PROJECTION_PROFILE, DATALAB_WORKSPACE_PROJECTION_SOURCE_CONTROLS,
        { 240.0f, 220.0f }
    };
    projection->nodes[DATALAB_WORKSPACE_PROJECTION_PROFILE] = (CorePaneNode){
        CORE_PANE_NODE_LEAF, DATALAB_WORKSPACE_SURFACE_PROFILE, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, {0.0f, 0.0f}
    };
    projection->nodes[DATALAB_WORKSPACE_PROJECTION_SOURCE_CONTROLS] = (CorePaneNode){
        CORE_PANE_NODE_LEAF, DATALAB_WORKSPACE_SURFACE_SOURCE_CONTROLS, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, {0.0f, 0.0f}
    };
}

void datalab_workspace_authoring_projection_capture_entry(DatalabAppState *state) {
    if (state) state->workspace_authoring_entry_projection = state->workspace_authoring_projection;
}

void datalab_workspace_authoring_projection_restore_entry(DatalabAppState *state) {
    if (state) state->workspace_authoring_projection = state->workspace_authoring_entry_projection;
}

int datalab_workspace_authoring_projection_apply_drag(DatalabAppState *state, float delta_x, float viewport_width) {
    CorePaneSplitterHit hit;
    if (!state || viewport_width <= 0.0f) return 0;
    memset(&hit, 0, sizeof(hit));
    hit.active = true;
    hit.node_index = DATALAB_WORKSPACE_PROJECTION_ROOT;
    hit.axis = CORE_PANE_AXIS_HORIZONTAL;
    hit.ratio_01 = state->workspace_authoring_projection.nodes[DATALAB_WORKSPACE_PROJECTION_ROOT].ratio_01;
    hit.parent_span = viewport_width;
    hit.min_ratio_01 = 240.0f / viewport_width;
    hit.max_ratio_01 = 1.0f - (220.0f / viewport_width);
    if (!core_pane_apply_splitter_drag(state->workspace_authoring_projection.nodes,
                                       DATALAB_WORKSPACE_PROJECTION_NODE_COUNT, &hit, delta_x, 0.0f)) return 0;
    state->workspace_authoring_projection.profile_surface_ratio =
        state->workspace_authoring_projection.nodes[DATALAB_WORKSPACE_PROJECTION_ROOT].ratio_01;
    state->workspace_authoring_pending_stub = 1u;
    return 1;
}

int datalab_workspace_authoring_projection_set_profile_surface_ratio(DatalabAppState *state, float ratio) {
    if (!state || ratio < 0.20f || ratio > 0.80f) return 0;
    state->workspace_authoring_projection.nodes[DATALAB_WORKSPACE_PROJECTION_ROOT].ratio_01 = ratio;
    state->workspace_authoring_projection.profile_surface_ratio = ratio;
    return 1;
}

int datalab_workspace_authoring_projection_solve(const DatalabAppState *state, int viewport_width,
                                                 int viewport_height, CorePaneLeafRect out_rects[2]) {
    uint32_t count = 0u;
    if (!state || !out_rects || viewport_width <= 0 || viewport_height <= 0) return 0;
    return core_pane_solve(state->workspace_authoring_projection.nodes,
                           DATALAB_WORKSPACE_PROJECTION_NODE_COUNT, DATALAB_WORKSPACE_PROJECTION_ROOT,
                           (CorePaneRect){0.0f, 0.0f, (float)viewport_width, (float)viewport_height},
                           out_rects, 2u, &count) && count == 2u;
}
