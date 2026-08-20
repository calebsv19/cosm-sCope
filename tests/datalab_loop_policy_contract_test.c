#include <stdio.h>
#include <string.h>

#include "app/app_state.h"
#include "render/render_view_internal.h"

static int datalab_test_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "loop-policy-contract: %s\n", message ? message : "assertion failed");
        return 0;
    }
    return 1;
}

static int test_wait_policy_prefers_idle_and_busy_modes(void) {
    DatalabLoopWaitPolicyInput input;
    memset(&input, 0, sizeof(input));

    if (!datalab_test_assert(datalab_loop_compute_wait_timeout_ms(&input) == 120,
                             "idle loop should use default wait timeout")) {
        return 0;
    }

    input.interaction_active = 1u;
    if (!datalab_test_assert(datalab_loop_compute_wait_timeout_ms(&input) == 8,
                             "interaction should force busy wait timeout")) {
        return 0;
    }

    memset(&input, 0, sizeof(input));
    input.background_busy = 1u;
    if (!datalab_test_assert(datalab_loop_compute_wait_timeout_ms(&input) == 8,
                             "background work should force busy wait timeout")) {
        return 0;
    }

    memset(&input, 0, sizeof(input));
    input.resize_pending = 1u;
    if (!datalab_test_assert(datalab_loop_compute_wait_timeout_ms(&input) == 8,
                             "resize pending should force busy wait timeout")) {
        return 0;
    }

    memset(&input, 0, sizeof(input));
    input.high_intensity_mode = 1u;
    if (!datalab_test_assert(datalab_loop_compute_wait_timeout_ms(&input) == 0,
                             "high intensity mode should disable waiting")) {
        return 0;
    }
    return 1;
}

static int test_wait_policy_update_tracks_runtime_signals(void) {
    DatalabAppState state;
    DatalabLoopWaitPolicyInput policy;
    DatalabInputFrame input_frame;

    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    memset(&policy, 0, sizeof(policy));
    datalab_input_frame_begin(&input_frame);
    input_frame.raw.sdl_event_count = 2u;

    datalab_loop_update_wait_policy_input(&policy, &input_frame, &state, 1, 1);
    if (!datalab_test_assert(policy.interaction_active == 1u,
                             "event activity should mark loop interaction active")) {
        return 0;
    }
    if (!datalab_test_assert(policy.background_busy == 1u,
                             "panel rescan pending should mark background busy")) {
        return 0;
    }
    if (!datalab_test_assert(policy.resize_pending == 1u,
                             "resize pending should propagate into wait policy")) {
        return 0;
    }

    memset(&policy, 0, sizeof(policy));
    datalab_input_frame_begin(&input_frame);
    state.workspace_authoring_pending_stub = 1u;
    datalab_loop_update_wait_policy_input(&policy, &input_frame, &state, 0, 0);
    if (!datalab_test_assert(policy.interaction_active == 1u,
                             "pending authoring work should keep interaction active")) {
        return 0;
    }
    if (!datalab_test_assert(policy.background_busy == 0u && policy.resize_pending == 0u,
                             "clear async flags should not mark busy or resize pending")) {
        return 0;
    }
    return 1;
}

static int test_render_reason_bits_cover_force_heartbeat_and_boundaries(void) {
    DatalabLoopBoundarySignals signals;
    uint32_t reason_bits;
    memset(&signals, 0, sizeof(signals));

    reason_bits = datalab_loop_compute_render_reason_bits(&signals, 0, 0u, 0u);
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_FORCE) != 0u,
                             "first frame should force a render")) {
        return 0;
    }
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_HEARTBEAT) != 0u,
                             "first frame should also carry heartbeat")) {
        return 0;
    }

    signals.sync_input_invalidated = 1u;
    signals.async_decode_frame_ready = 1u;
    signals.async_panel_rescan_pending = 1u;
    signals.async_authoring_pending = 1u;
    reason_bits = datalab_loop_compute_render_reason_bits(&signals, 1, 100u, 120u);
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_INPUT_INVALIDATE) != 0u,
                             "input invalidation should request render")) {
        return 0;
    }
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_ASYNC_DECODE_READY) != 0u,
                             "a staged async image must request an immediate in-session render")) {
        return 0;
    }
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_ASYNC_PANEL_RESCAN) != 0u,
                             "panel rescan pending should request render")) {
        return 0;
    }
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_ASYNC_AUTHORING) != 0u,
                             "authoring pending should request render")) {
        return 0;
    }
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_RESIZE) != 0u,
                             "resize pending should request render")) {
        return 0;
    }
    if (!datalab_test_assert((reason_bits & DATALAB_LOOP_RENDER_REASON_HEARTBEAT) == 0u,
                             "sub-heartbeat frame should not add heartbeat reason")) {
        return 0;
    }

    memset(&signals, 0, sizeof(signals));
    reason_bits = datalab_loop_compute_render_reason_bits(&signals, 0, 100u, 351u);
    if (!datalab_test_assert(reason_bits == DATALAB_LOOP_RENDER_REASON_HEARTBEAT,
                             "idle heartbeat interval should request heartbeat-only render")) {
        return 0;
    }

    reason_bits = datalab_loop_compute_render_reason_bits(NULL, 1, 0u, 0u);
    if (!datalab_test_assert(reason_bits == 0u,
                             "null boundary signals should safely produce no render reasons")) {
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_wait_policy_prefers_idle_and_busy_modes()) {
        return 1;
    }
    if (!test_wait_policy_update_tracks_runtime_signals()) {
        return 1;
    }
    if (!test_render_reason_bits_cover_force_heartbeat_and_boundaries()) {
        return 1;
    }
    puts("datalab loop policy contract test passed");
    return 0;
}
