#include <math.h>
#include <stdio.h>

#include "app/app_state.h"
#include "render/render_view_internal.h"

static int datalab_test_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "raster-viewport-contract: %s\n", message ? message : "assertion failed");
        return 0;
    }
    return 1;
}

static int datalab_test_float_eq(float actual, float expected, float tolerance, const char *message) {
    if (fabsf(actual - expected) > tolerance) {
        fprintf(stderr,
                "raster-viewport-contract: %s (actual=%f expected=%f)\n",
                message ? message : "float mismatch",
                (double)actual,
                (double)expected);
        return 0;
    }
    return 1;
}

static int test_request_reset_clears_drag_state(void) {
    DatalabRasterViewportState state;
    datalab_raster_viewport_state_init(&state);
    state.valid = 1;
    state.fit_mode = 0;
    state.reset_requested = 0;
    state.drag_active = 1;

    datalab_raster_viewport_request_reset(&state);
    if (!datalab_test_assert(state.fit_mode == 1, "request reset should restore fit mode")) {
        return 0;
    }
    if (!datalab_test_assert(state.reset_requested == 1, "request reset should mark reset pending")) {
        return 0;
    }
    if (!datalab_test_assert(state.drag_active == 0, "request reset should clear active drag")) {
        return 0;
    }
    return 1;
}

static int test_sync_bootstraps_fit_and_clears_drag(void) {
    DatalabRasterViewportState state;
    datalab_raster_viewport_state_init(&state);
    state.drag_active = 1;

    datalab_raster_viewport_sync_state(&state, 800, 600, 400u, 200u);
    if (!datalab_test_assert(state.valid == 1, "sync should validate a fresh raster viewport")) {
        return 0;
    }
    if (!datalab_test_assert(state.fit_mode == 1, "initial sync should stay in fit mode")) {
        return 0;
    }
    if (!datalab_test_assert(state.reset_requested == 0, "initial sync should clear pending reset")) {
        return 0;
    }
    if (!datalab_test_assert(state.drag_active == 0, "fit bootstrap should clear drag state")) {
        return 0;
    }
    if (!datalab_test_assert(state.view_width == 800 && state.view_height == 600,
                             "sync should store current view size")) {
        return 0;
    }
    if (!datalab_test_assert(state.content_width == 400u && state.content_height == 200u,
                             "sync should store current content size")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.zoom, 2.0f, 0.0001f, "fit bootstrap zoom should match view")) {
        return 0;
    }
    return 1;
}

static int test_content_change_forces_fit_reset(void) {
    DatalabRasterViewportState state;
    datalab_raster_viewport_state_init(&state);
    datalab_raster_viewport_sync_state(&state, 800, 600, 400u, 200u);
    state.fit_mode = 0;
    state.reset_requested = 0;
    state.drag_active = 1;
    state.viewport.zoom = 3.5f;
    state.viewport.pan_x = 19.0f;
    state.viewport.pan_y = -7.0f;

    datalab_raster_viewport_sync_state(&state, 800, 600, 640u, 320u);
    if (!datalab_test_assert(state.fit_mode == 1, "content change should restore fit mode")) {
        return 0;
    }
    if (!datalab_test_assert(state.reset_requested == 0, "content change fit should complete reset immediately")) {
        return 0;
    }
    if (!datalab_test_assert(state.drag_active == 0, "content change fit should clear drag state")) {
        return 0;
    }
    if (!datalab_test_assert(state.content_width == 640u && state.content_height == 320u,
                             "content change should update tracked content size")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.zoom, 1.25f, 0.0001f, "content change should recompute fit zoom")) {
        return 0;
    }
    return 1;
}

static int test_free_view_resize_preserves_manual_viewport(void) {
    DatalabRasterViewportState state;
    datalab_raster_viewport_state_init(&state);
    datalab_raster_viewport_sync_state(&state, 800, 600, 400u, 200u);
    state.fit_mode = 0;
    state.reset_requested = 0;
    state.drag_active = 1;
    state.viewport.zoom = 2.75f;
    state.viewport.pan_x = 48.0f;
    state.viewport.pan_y = -32.0f;

    datalab_raster_viewport_sync_state(&state, 900, 700, 400u, 200u);
    if (!datalab_test_assert(state.fit_mode == 0, "free view resize should stay out of fit mode")) {
        return 0;
    }
    if (!datalab_test_assert(state.reset_requested == 0, "free view resize should not request a reset")) {
        return 0;
    }
    if (!datalab_test_assert(state.drag_active == 1, "free view resize should not clear drag state by itself")) {
        return 0;
    }
    if (!datalab_test_assert(state.view_width == 900 && state.view_height == 700,
                             "free view resize should update tracked view size")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.zoom, 2.75f, 0.0001f, "free view resize should preserve zoom")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.pan_x, 48.0f, 0.0001f, "free view resize should preserve pan_x")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.pan_y, -32.0f, 0.0001f, "free view resize should preserve pan_y")) {
        return 0;
    }
    return 1;
}

static int test_manual_zoom_and_drag_helpers_switch_to_free_view(void) {
    DatalabRasterViewportState state;
    datalab_raster_viewport_state_init(&state);
    datalab_raster_viewport_sync_state(&state, 800, 600, 400u, 200u);

    if (!datalab_test_assert(datalab_raster_viewport_zoom_at_screen_anchor(&state,
                                                                           200,
                                                                           150,
                                                                           1.5f) == 1,
                             "zoom helper should apply valid anchored zoom")) {
        return 0;
    }
    if (!datalab_test_assert(state.fit_mode == 0 && state.reset_requested == 0 && state.drag_active == 0,
                             "zoom helper should switch to free view and clear drag")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.zoom, 3.0f, 0.0001f, "zoom helper should use core viewport zoom")) {
        return 0;
    }

    if (!datalab_test_assert(datalab_raster_viewport_begin_drag(&state, 100, 90) == 1,
                             "begin drag helper should activate drag on valid viewport")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_raster_viewport_drag_to(&state, 130, 120) == 1,
                             "drag helper should pan a valid active drag")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.pan_x, -70.0f, 0.0001f, "drag helper should pan x through core viewport")) {
        return 0;
    }
    if (!datalab_test_float_eq(state.viewport.pan_y, 105.0f, 0.0001f, "drag helper should pan y through core viewport")) {
        return 0;
    }
    if (!datalab_test_assert(state.last_mouse_x == 130 && state.last_mouse_y == 120,
                             "drag helper should update the last pointer anchor")) {
        return 0;
    }
    datalab_raster_viewport_end_drag(&state);
    if (!datalab_test_assert(state.drag_active == 0, "end drag helper should clear drag state")) {
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_request_reset_clears_drag_state()) {
        return 1;
    }
    if (!test_sync_bootstraps_fit_and_clears_drag()) {
        return 1;
    }
    if (!test_content_change_forces_fit_reset()) {
        return 1;
    }
    if (!test_free_view_resize_preserves_manual_viewport()) {
        return 1;
    }
    if (!test_manual_zoom_and_drag_helpers_switch_to_free_view()) {
        return 1;
    }
    puts("datalab raster viewport contract test passed");
    return 0;
}
