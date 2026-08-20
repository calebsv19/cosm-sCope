#include <stdio.h>

#include "app/datalab_focus_window.h"

static int require(int condition, const char *message) {
    if (!condition) fprintf(stderr, "focus-window-contract: %s\n", message);
    return condition;
}

int main(void) {
    DatalabFocusWindow window;
    DatalabFocusWindowIntent intent;
    const DatalabFocusWindowMetrics *metrics = NULL;
    uint64_t last_index = 0u;
    int selected_seen = 0;

    datalab_focus_window_init(&window);
    datalab_focus_window_set_active(&window, 500000u);
    datalab_focus_window_select(&window, 7u, 1000000u, 500000u, 1, 1u, 0);
    metrics = datalab_focus_window_metrics(&window);
    if (!require(metrics->queued == 5u && metrics->radius == 2u &&
                 metrics->active_index == 500000u,
                 "million-entry catalog must produce only selected plus four neighbor intents")) return 1;

    for (uint64_t index = 500001u; index <= 501000u; ++index)
        datalab_focus_window_select(&window, 7u, 1000000u, index, 1, 1000u, 1);
    while (datalab_focus_window_pop_intent(&window, &intent)) {
        if (intent.kind == DATALAB_FOCUS_WINDOW_INTENT_SELECTED) {
            last_index = intent.logical_index;
            selected_seen++;
        }
        datalab_focus_window_note_complete(&window, &intent, 1);
    }
    metrics = datalab_focus_window_metrics(&window);
    if (!require(selected_seen == 1 && last_index == 501000u &&
                 metrics->peak_queued <= DATALAB_FOCUS_WINDOW_INTENT_CAPACITY &&
                 metrics->queued == 0u && metrics->inflight == 0u,
                 "rapid thousand-step scrub must coalesce to one bounded latest focus window")) return 1;

    datalab_focus_window_set_pending(&window, 501000u);
    datalab_focus_window_set_pressure(&window, DATALAB_FOCUS_WINDOW_PRESSURE_CRITICAL);
    datalab_focus_window_select(&window, 7u, 1000000u, 501001u, 1, 1u, 0);
    metrics = datalab_focus_window_metrics(&window);
    if (!require(metrics->radius == 0u && metrics->queued == 1u &&
                 metrics->pending_index == UINT64_MAX && metrics->active_index == 500000u,
                 "critical pressure must shrink to selected-only and never evict active")) return 1;

    if (!require(datalab_focus_window_pop_intent(&window, &intent),
                 "selected-only pressure window must yield one intent")) return 1;
    datalab_focus_window_select(&window, 8u, 1000000u, 10u, -1, 4u, 1);
    datalab_focus_window_note_complete(&window, &intent, 0);
    while (datalab_focus_window_pop_intent(&window, &intent))
        datalab_focus_window_note_complete(&window, &intent, intent.catalog_generation == 8u);
    metrics = datalab_focus_window_metrics(&window);
    if (!require(metrics->catalog_generation == 8u && metrics->stale > 0u &&
                 metrics->active_index == 500000u,
                 "generation rescan must reject stale work without changing displayed active frame")) return 1;
    puts("datalab focus-window contract test passed");
    return 0;
}
