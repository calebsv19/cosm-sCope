#include "render/render_view_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "app/datalab_input_catalog.h"

#define DATALAB_PLAYBACK_STEP_INTERVAL_MS_DEFAULT DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT

size_t datalab_panel_find_active_index(const DatalabPackPanelCache *cache, const char *active_path) {
    const char *active_name = NULL;
    size_t i = 0u;
    if (!cache || !active_path || active_path[0] == '\0') {
        return (size_t)-1;
    }
    active_name = core_path_basename(active_path);
    if (!active_name || active_name[0] == '\0') {
        return (size_t)-1;
    }
    if (cache->source_catalog) {
        uint64_t index = 0u;
        return datalab_input_catalog_find_index(cache->source_catalog, active_name, &index) ? (size_t)index : (size_t)-1;
    }
    for (i = 0u; i < cache->file_count && i < DATALAB_PANEL_VISIBLE_WINDOW; ++i)
        if (strcasecmp(cache->files[i], active_name) == 0) return i;
    return (size_t)-1;
}

static int datalab_panel_name_copy(const DatalabPackPanelCache *cache,
                                   size_t index,
                                   char *out_name,
                                   size_t out_name_cap) {
    if (!cache || !out_name || out_name_cap == 0u || index >= cache->file_count) return 0;
    out_name[0] = '\0';
    if (cache->source_catalog)
        return datalab_input_catalog_name_copy(cache->source_catalog, (uint64_t)index, out_name, out_name_cap);
    if (index >= DATALAB_PANEL_VISIBLE_WINDOW) return 0;
    snprintf(out_name, out_name_cap, "%s", cache->files[index]);
    return 1;
}

static int datalab_playback_valid_direction(int direction) {
    return direction < 0 ? -1 : 1;
}

static void datalab_panel_apply_loop_step(DatalabAppState *app_state,
                                          const DatalabPackPanelCache *cache) {
    int64_t idx = 0;
    int direction = 1;
    if (!app_state || !cache || cache->file_count == 0u) {
        return;
    }
    direction = datalab_playback_valid_direction(app_state->playback_direction);
    idx = (int64_t)app_state->panel_selected_index + (int64_t)direction;
    if (idx < 0) {
        idx = (int64_t)cache->file_count - 1;
    } else if ((size_t)idx >= cache->file_count) {
        idx = 0;
    }
    app_state->panel_selected_index = (size_t)idx;
    app_state->playback_direction = direction;
}

static void datalab_panel_apply_bounce_step(DatalabAppState *app_state,
                                            const DatalabPackPanelCache *cache) {
    int direction = 1;
    if (!app_state || !cache || cache->file_count == 0u) {
        return;
    }
    direction = datalab_playback_valid_direction(app_state->playback_direction);
    if (cache->file_count == 1u) {
        app_state->panel_selected_index = 0u;
        app_state->playback_direction = 1;
        return;
    }
    if (direction > 0 && app_state->panel_selected_index + 1u >= cache->file_count) {
        direction = -1;
    } else if (direction < 0 && app_state->panel_selected_index == 0u) {
        direction = 1;
    }
    app_state->panel_selected_index =
        (size_t)((long)app_state->panel_selected_index + (long)direction);
    app_state->playback_direction = direction;
}

static void datalab_panel_apply_playback_step(DatalabAppState *app_state,
                                              const DatalabPackPanelCache *cache) {
    if (!app_state || !cache || cache->file_count == 0u) {
        return;
    }
    switch (app_state->playback_mode) {
        case DATALAB_PLAYBACK_MODE_BOUNCE:
            datalab_panel_apply_bounce_step(app_state, cache);
            break;
        case DATALAB_PLAYBACK_MODE_LOOP:
        default:
            app_state->playback_mode = DATALAB_PLAYBACK_MODE_LOOP;
            datalab_panel_apply_loop_step(app_state, cache);
            break;
    }
}

static size_t datalab_panel_selection_index_after_delta(size_t selected_index,
                                                        int delta,
                                                        size_t file_count) {
    int64_t count = 0;
    int64_t idx = 0;
    if (file_count == 0u) {
        return 0u;
    }
    count = (int64_t)file_count;
    idx = selected_index >= file_count ? count - 1 : (int64_t)selected_index;
    idx = (idx + (int64_t)delta) % count;
    if (idx < 0) {
        idx += count;
    }
    return (size_t)idx;
}

void datalab_panel_apply_state(DatalabAppState *app_state,
                               DatalabPackPanelCache *cache,
                               const char *root,
                               int rescanned,
                               uint32_t now_ticks) {
    if (!datalab_workspace_authoring_runtime_mutation_allowed(app_state) || !cache) {
        return;
    }
    if (!root || root[0] == '\0') {
        cache->file_count = 0u;
        cache->source_catalog = NULL;
        cache->source_catalog_file_count = 0u;
        cache->scanned_root[0] = '\0';
        cache->last_scan_ticks = 0u;
        snprintf(cache->status, sizeof(cache->status), "no input root selected (press O)");
        datalab_panel_reset_interaction_state(app_state);
        datalab_playback_stop(app_state);
        return;
    }

    if (rescanned && cache->file_count > 0u) {
        size_t active_index = datalab_panel_find_active_index(cache, app_state->pack_path);
        if (active_index != (size_t)-1) {
            app_state->panel_selected_index = active_index;
        }
    }

    if (cache->file_count == 0u) {
        app_state->panel_selected_index = 0u;
        app_state->panel_selection_delta = 0;
        app_state->panel_selection_home_requested = 0;
        app_state->panel_selection_end_requested = 0;
        datalab_playback_stop(app_state);
    } else {
        int delta = app_state->panel_selection_delta;
        if (app_state->panel_selection_home_requested) {
            app_state->panel_selected_index = 0u;
            app_state->panel_selection_home_requested = 0;
        } else if (app_state->panel_selection_end_requested) {
            app_state->panel_selected_index = cache->file_count - 1u;
            app_state->panel_selection_end_requested = 0;
        } else if (delta != 0) {
            app_state->panel_selected_index =
                datalab_panel_selection_index_after_delta(app_state->panel_selected_index,
                                                          delta,
                                                          cache->file_count);
            app_state->panel_selection_delta = 0;
        } else if (app_state->panel_selected_index >= cache->file_count) {
            app_state->panel_selected_index = cache->file_count - 1u;
        }
    }

    if (app_state->playback_active && cache->file_count > 0u) {
        uint32_t step_interval_ms = app_state->playback_interval_ms;
        if (step_interval_ms == 0u) {
            if (app_state->playback_speed_index < DATALAB_PLAYBACK_SPEED_INDEX_MIN ||
                app_state->playback_speed_index > DATALAB_PLAYBACK_SPEED_INDEX_MAX) {
                app_state->playback_speed_index = DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT;
                step_interval_ms = DATALAB_PLAYBACK_STEP_INTERVAL_MS_DEFAULT;
            } else {
                step_interval_ms =
                    datalab_playback_interval_for_speed_index(app_state->playback_speed_index);
            }
            app_state->playback_interval_ms = step_interval_ms;
        }
        if ((uint32_t)(now_ticks - app_state->playback_last_advance_ticks) >= step_interval_ms) {
            datalab_panel_apply_playback_step(app_state, cache);
            app_state->panel_open_selected_requested = 1;
            app_state->playback_last_advance_ticks = now_ticks;
        }
    }

    if (app_state->panel_open_selected_requested) {
        app_state->panel_open_selected_requested = 0;
        if (cache->file_count > 0u && app_state->panel_selected_index < cache->file_count) {
            char selected_name[DATALAB_APP_PATH_CAP];
            if (!datalab_panel_name_copy(cache, app_state->panel_selected_index, selected_name, sizeof(selected_name)) ||
                (app_state->input_catalog &&
                 !datalab_input_catalog_file_is_current(app_state->input_catalog, root, selected_name)) ||
                !datalab_panel_request_pack_under_root(app_state, root, selected_name)) {
                snprintf(cache->status, sizeof(cache->status), "selected file is outside input root");
                datalab_playback_stop(app_state);
            }
        } else {
            datalab_panel_request_pack_path(app_state, NULL);
            snprintf(cache->status, sizeof(cache->status), "no file selected");
        }
    }
}
