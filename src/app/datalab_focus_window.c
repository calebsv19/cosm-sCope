#include "app/datalab_focus_window.h"

#include <string.h>

static uint32_t datalab_focus_window_radius(const DatalabFocusWindow *window) {
    uint32_t radius = DATALAB_FOCUS_WINDOW_DEFAULT_RADIUS;
    if (!window) return 0u;
    if (window->pressure == DATALAB_FOCUS_WINDOW_PRESSURE_CRITICAL) return 0u;
    if (window->pressure == DATALAB_FOCUS_WINDOW_PRESSURE_ELEVATED) return 1u;
    if (window->playback_active && window->velocity >= 4u) radius = 3u;
    return radius > DATALAB_FOCUS_WINDOW_MAX_RADIUS ? DATALAB_FOCUS_WINDOW_MAX_RADIUS : radius;
}

static void datalab_focus_window_clear_queue(DatalabFocusWindow *window) {
    if (!window) return;
    window->metrics.cancelled += window->metrics.queued;
    window->metrics.queued = 0u;
}

static void datalab_focus_window_update_peaks(DatalabFocusWindow *window) {
    if (!window) return;
    if (window->metrics.queued > window->metrics.peak_queued)
        window->metrics.peak_queued = (uint32_t)window->metrics.queued;
    if (window->metrics.inflight > window->metrics.peak_inflight)
        window->metrics.peak_inflight = (uint32_t)window->metrics.inflight;
}

static void datalab_focus_window_enqueue(DatalabFocusWindow *window,
                                         uint64_t index,
                                         DatalabFocusWindowIntentKind kind,
                                         uint8_t priority) {
    DatalabFocusWindowIntent *intent = NULL;
    if (!window || index >= window->metrics.catalog_count) return;
    for (uint64_t i = 0u; i < window->metrics.queued; ++i) {
        if (window->intents[i].catalog_generation == window->metrics.catalog_generation &&
            window->intents[i].logical_index == index) {
            window->metrics.deduplicated++;
            return;
        }
    }
    if (window->metrics.queued >= DATALAB_FOCUS_WINDOW_INTENT_CAPACITY) {
        window->metrics.cancelled++;
        return;
    }
    intent = &window->intents[window->metrics.queued++];
    intent->catalog_generation = window->metrics.catalog_generation;
    intent->logical_index = index;
    intent->kind = kind;
    intent->priority = priority;
    window->metrics.intents_emitted++;
    datalab_focus_window_update_peaks(window);
}

void datalab_focus_window_init(DatalabFocusWindow *window) {
    if (!window) return;
    memset(window, 0, sizeof(*window));
    window->metrics.active_index = UINT64_MAX;
    window->metrics.pending_index = UINT64_MAX;
    window->direction = 1;
    window->pressure = DATALAB_FOCUS_WINDOW_PRESSURE_NORMAL;
    window->metrics.radius = DATALAB_FOCUS_WINDOW_DEFAULT_RADIUS;
}

void datalab_focus_window_set_active(DatalabFocusWindow *window, uint64_t logical_index) {
    if (!window) return;
    window->metrics.active_index = logical_index;
    window->has_active = 1;
}

void datalab_focus_window_set_pending(DatalabFocusWindow *window, uint64_t logical_index) {
    if (!window) return;
    window->metrics.pending_index = logical_index;
    window->has_pending = 1;
}

void datalab_focus_window_commit_pending(DatalabFocusWindow *window) {
    if (!window || !window->has_pending) return;
    datalab_focus_window_set_active(window, window->metrics.pending_index);
    window->metrics.pending_index = UINT64_MAX;
    window->has_pending = 0;
}

void datalab_focus_window_cancel_pending(DatalabFocusWindow *window) {
    if (!window || !window->has_pending) return;
    window->metrics.cancelled++;
    window->metrics.pending_index = UINT64_MAX;
    window->has_pending = 0;
}

void datalab_focus_window_set_pressure(DatalabFocusWindow *window,
                                       DatalabFocusWindowPressure pressure) {
    if (!window) return;
    window->pressure = pressure;
    window->recovery_ticks = 0u;
    window->metrics.radius = datalab_focus_window_radius(window);
    if (pressure != DATALAB_FOCUS_WINDOW_PRESSURE_NORMAL) datalab_focus_window_clear_queue(window);
}

void datalab_focus_window_select(DatalabFocusWindow *window,
                                 uint64_t catalog_generation,
                                 uint64_t catalog_count,
                                 uint64_t logical_index,
                                 int direction,
                                 uint32_t velocity,
                                 int playback_active) {
    uint32_t radius = 0u;
    int step = 0;
    if (!window || catalog_count == 0u || logical_index >= catalog_count) return;
    if (window->metrics.catalog_generation != catalog_generation ||
        window->metrics.catalog_count != catalog_count) {
        datalab_focus_window_clear_queue(window);
        if (window->metrics.inflight) window->metrics.stale += window->metrics.inflight;
        window->metrics.catalog_generation = catalog_generation;
        window->metrics.catalog_count = catalog_count;
    } else {
        datalab_focus_window_clear_queue(window);
    }
    datalab_focus_window_cancel_pending(window);
    window->direction = direction < 0 ? -1 : 1;
    window->velocity = velocity;
    window->playback_active = playback_active ? 1 : 0;
    radius = datalab_focus_window_radius(window);
    window->metrics.radius = radius;
    window->metrics.selection_updates++;
    datalab_focus_window_enqueue(window, logical_index, DATALAB_FOCUS_WINDOW_INTENT_SELECTED, 0u);
    for (uint32_t distance = 1u; distance <= radius; ++distance) {
        uint64_t forward = logical_index;
        uint64_t reverse = logical_index;
        if (window->direction > 0) {
            if (logical_index + distance < catalog_count) forward = logical_index + distance;
            else forward = UINT64_MAX;
            if (logical_index >= distance) reverse = logical_index - distance;
            else reverse = UINT64_MAX;
        } else {
            if (logical_index >= distance) forward = logical_index - distance;
            else forward = UINT64_MAX;
            if (logical_index + distance < catalog_count) reverse = logical_index + distance;
            else reverse = UINT64_MAX;
        }
        if (forward != UINT64_MAX)
            datalab_focus_window_enqueue(window, forward, DATALAB_FOCUS_WINDOW_INTENT_NEIGHBOR, (uint8_t)(distance * 2u - 1u));
        if (reverse != UINT64_MAX)
            datalab_focus_window_enqueue(window, reverse, DATALAB_FOCUS_WINDOW_INTENT_NEIGHBOR, (uint8_t)(distance * 2u));
    }
    /* Keep C's signed arithmetic out of the index path. */
    (void)step;
}

int datalab_focus_window_pop_intent(DatalabFocusWindow *window,
                                    DatalabFocusWindowIntent *out_intent) {
    if (!window || !out_intent || window->metrics.queued == 0u) return 0;
    *out_intent = window->intents[0];
    if (window->metrics.queued > 1u)
        memmove(&window->intents[0], &window->intents[1],
                (size_t)(window->metrics.queued - 1u) * sizeof(window->intents[0]));
    window->metrics.queued--;
    window->metrics.inflight++;
    datalab_focus_window_update_peaks(window);
    return 1;
}

void datalab_focus_window_note_complete(DatalabFocusWindow *window,
                                       const DatalabFocusWindowIntent *intent,
                                       int current) {
    if (!window || !intent) return;
    if (window->metrics.inflight) window->metrics.inflight--;
    if (!current || intent->catalog_generation != window->metrics.catalog_generation) {
        window->metrics.stale++;
        return;
    }
    window->metrics.completed++;
    if (window->pressure == DATALAB_FOCUS_WINDOW_PRESSURE_NORMAL &&
        window->metrics.radius < DATALAB_FOCUS_WINDOW_DEFAULT_RADIUS &&
        ++window->recovery_ticks >= 8u) {
        window->recovery_ticks = 0u;
        window->metrics.radius++;
    }
}

const DatalabFocusWindowMetrics *datalab_focus_window_metrics(const DatalabFocusWindow *window) {
    return window ? &window->metrics : NULL;
}
