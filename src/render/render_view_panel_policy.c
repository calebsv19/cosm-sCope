#include "render/render_view_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

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
    for (i = 0u; i < cache->file_count; ++i) {
        if (strcasecmp(cache->files[i], active_name) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

static int datalab_playback_valid_direction(int direction) {
    return direction < 0 ? -1 : 1;
}

static void datalab_panel_apply_loop_step(DatalabAppState *app_state,
                                          const DatalabPackPanelCache *cache) {
    long idx = 0;
    int direction = 1;
    if (!app_state || !cache || cache->file_count == 0u) {
        return;
    }
    direction = datalab_playback_valid_direction(app_state->playback_direction);
    idx = (long)app_state->panel_selected_index + (long)direction;
    if (idx < 0) {
        idx = (long)cache->file_count - 1L;
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
    long count = 0;
    long idx = 0;
    if (file_count == 0u) {
        return 0u;
    }
    count = (long)file_count;
    idx = selected_index >= file_count ? count - 1L : (long)selected_index;
    idx = (idx + (long)delta) % count;
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
    if (!app_state || !cache) {
        return;
    }
    if (!root || root[0] == '\0') {
        cache->file_count = 0u;
        cache->scanned_root[0] = '\0';
        cache->last_scan_ticks = 0u;
        snprintf(cache->status, sizeof(cache->status), "no input root selected (press O)");
        app_state->panel_selected_index = 0u;
        app_state->panel_selection_delta = 0;
        app_state->panel_open_selected_requested = 0;
        app_state->panel_requested_pack_path[0] = '\0';
        app_state->playback_active = 0;
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
        app_state->playback_active = 0;
    } else {
        int delta = app_state->panel_selection_delta;
        if (delta != 0) {
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
        app_state->panel_requested_pack_path[0] = '\0';
        if (cache->file_count > 0u && app_state->panel_selected_index < cache->file_count) {
            snprintf(app_state->panel_requested_pack_path,
                     sizeof(app_state->panel_requested_pack_path),
                     "%s/%s",
                     root,
                     cache->files[app_state->panel_selected_index]);
        } else {
            snprintf(cache->status, sizeof(cache->status), "no file selected");
        }
    }
}
