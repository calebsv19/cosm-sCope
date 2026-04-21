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
