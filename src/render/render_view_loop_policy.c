#include "render/render_view_internal.h"

#include <stdlib.h>

enum {
    DATALAB_LOOP_WAIT_IDLE_DEFAULT_MS = 120,
    DATALAB_LOOP_WAIT_BUSY_MS = 8,
    DATALAB_LOOP_WAIT_MIN_MS = 1,
    DATALAB_LOOP_WAIT_MAX_MS = 5000
};

static int datalab_loop_env_wait_override_ms(void) {
    const char *wait_env = getenv("DATALAB_LOOP_MAX_WAIT_MS");
    char *end = NULL;
    long parsed = 0;
    if (!wait_env || wait_env[0] == '\0') {
        return -1;
    }
    parsed = strtol(wait_env, &end, 10);
    if (end == wait_env ||
        parsed < DATALAB_LOOP_WAIT_MIN_MS ||
        parsed > DATALAB_LOOP_WAIT_MAX_MS) {
        return -1;
    }
    return (int)parsed;
}

int datalab_loop_compute_wait_timeout_ms(const DatalabLoopWaitPolicyInput *input) {
    int timeout_ms = DATALAB_LOOP_WAIT_IDLE_DEFAULT_MS;
    int env_override = -1;
    if (!input) {
        return 0;
    }
    if (input->high_intensity_mode) {
        return 0;
    }
    if (input->interaction_active ||
        input->background_busy ||
        input->resize_pending) {
        timeout_ms = DATALAB_LOOP_WAIT_BUSY_MS;
    }
    env_override = datalab_loop_env_wait_override_ms();
    if (env_override > 0 && timeout_ms > env_override) {
        timeout_ms = env_override;
    }
    if (timeout_ms < DATALAB_LOOP_WAIT_MIN_MS) {
        timeout_ms = DATALAB_LOOP_WAIT_MIN_MS;
    }
    if (timeout_ms > DATALAB_LOOP_WAIT_MAX_MS) {
        timeout_ms = DATALAB_LOOP_WAIT_MAX_MS;
    }
    return timeout_ms;
}

void datalab_loop_update_wait_policy_input(DatalabLoopWaitPolicyInput *policy,
                                           const DatalabInputFrame *input_frame,
                                           const DatalabAppState *app_state,
                                           int panel_rescan_pending,
                                           int resize_pending) {
    if (!policy || !input_frame || !app_state) {
        return;
    }
    policy->high_intensity_mode = 0u;
    policy->interaction_active = (input_frame->raw.sdl_event_count > 0u || app_state->workspace_authoring_pending_stub)
                                     ? 1u
                                     : 0u;
    policy->background_busy = panel_rescan_pending ? 1u : 0u;
    policy->resize_pending = resize_pending ? 1u : 0u;
}

uint32_t datalab_loop_compute_render_reason_bits(const DatalabLoopBoundarySignals *signals,
                                                 int resize_pending,
                                                 uint32_t last_present_ticks,
                                                 uint32_t now_ticks) {
    uint32_t reason_bits = 0u;
    if (!signals) {
        return 0u;
    }
    if (last_present_ticks == 0u) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_FORCE;
    }
    if (signals->sync_input_invalidated) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_INPUT_INVALIDATE;
    }
    if (signals->async_panel_rescan_pending) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_ASYNC_PANEL_RESCAN;
    }
    if (signals->async_authoring_pending) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_ASYNC_AUTHORING;
    }
    if (resize_pending) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_RESIZE;
    }
    if (last_present_ticks == 0u ||
        (uint32_t)(now_ticks - last_present_ticks) >= DATALAB_LOOP_RENDER_HEARTBEAT_MS) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_HEARTBEAT;
    }
    return reason_bits;
}
