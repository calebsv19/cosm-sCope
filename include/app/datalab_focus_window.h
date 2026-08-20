#ifndef DATALAB_FOCUS_WINDOW_H
#define DATALAB_FOCUS_WINDOW_H

#include <stddef.h>
#include <stdint.h>

/*
 * The focus window is catalog-index-only: it owns no paths, decoded pixels,
 * textures, or catalog copies.  The runtime resolves its few intents against
 * the current catalog immediately before work is submitted.
 */
enum {
    DATALAB_FOCUS_WINDOW_DEFAULT_RADIUS = 2,
    DATALAB_FOCUS_WINDOW_MAX_RADIUS = 4,
    DATALAB_FOCUS_WINDOW_INTENT_CAPACITY = 9
};

typedef enum DatalabFocusWindowPressure {
    DATALAB_FOCUS_WINDOW_PRESSURE_NORMAL = 0,
    DATALAB_FOCUS_WINDOW_PRESSURE_ELEVATED,
    DATALAB_FOCUS_WINDOW_PRESSURE_CRITICAL
} DatalabFocusWindowPressure;

typedef enum DatalabFocusWindowIntentKind {
    DATALAB_FOCUS_WINDOW_INTENT_SELECTED = 0,
    DATALAB_FOCUS_WINDOW_INTENT_NEIGHBOR
} DatalabFocusWindowIntentKind;

typedef struct DatalabFocusWindowIntent {
    uint64_t catalog_generation;
    uint64_t logical_index;
    DatalabFocusWindowIntentKind kind;
    uint8_t priority;
} DatalabFocusWindowIntent;

typedef struct DatalabFocusWindowMetrics {
    uint64_t active_index;
    uint64_t pending_index;
    uint64_t catalog_generation;
    uint64_t catalog_count;
    uint64_t queued;
    uint64_t inflight;
    uint64_t completed;
    uint64_t cancelled;
    uint64_t stale;
    uint64_t intents_emitted;
    uint64_t deduplicated;
    uint64_t selection_updates;
    uint32_t radius;
    uint32_t peak_queued;
    uint32_t peak_inflight;
} DatalabFocusWindowMetrics;

typedef struct DatalabFocusWindow {
    DatalabFocusWindowIntent intents[DATALAB_FOCUS_WINDOW_INTENT_CAPACITY];
    DatalabFocusWindowMetrics metrics;
    int direction;
    uint32_t velocity;
    int playback_active;
    DatalabFocusWindowPressure pressure;
    uint32_t recovery_ticks;
    int has_active;
    int has_pending;
} DatalabFocusWindow;

void datalab_focus_window_init(DatalabFocusWindow *window);
void datalab_focus_window_set_active(DatalabFocusWindow *window, uint64_t logical_index);
void datalab_focus_window_set_pending(DatalabFocusWindow *window, uint64_t logical_index);
void datalab_focus_window_commit_pending(DatalabFocusWindow *window);
void datalab_focus_window_cancel_pending(DatalabFocusWindow *window);
void datalab_focus_window_set_pressure(DatalabFocusWindow *window,
                                       DatalabFocusWindowPressure pressure);
/* A newer catalog generation invalidates every outstanding intent. */
void datalab_focus_window_select(DatalabFocusWindow *window,
                                 uint64_t catalog_generation,
                                 uint64_t catalog_count,
                                 uint64_t logical_index,
                                 int direction,
                                 uint32_t velocity,
                                 int playback_active);
int datalab_focus_window_pop_intent(DatalabFocusWindow *window,
                                    DatalabFocusWindowIntent *out_intent);
void datalab_focus_window_note_complete(DatalabFocusWindow *window,
                                       const DatalabFocusWindowIntent *intent,
                                       int current);
const DatalabFocusWindowMetrics *datalab_focus_window_metrics(const DatalabFocusWindow *window);

#endif
