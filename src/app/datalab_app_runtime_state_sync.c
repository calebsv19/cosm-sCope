#include "app/datalab_app_internal.h"
#include "app/datalab_runtime_prefs.h"

#include <SDL2/SDL.h>

#include <stdio.h>

void datalab_runtime_copy_to_app_state(const DatalabAppRuntime *runtime,
                                       DatalabAppState *app_state,
                                       int panel_rescan_requested) {
    int i = 0;
    if (!runtime || !app_state) {
        return;
    }

    datalab_app_state_init(app_state, runtime->pack_path, runtime->frame.profile);
    app_state->text_zoom_step = runtime->text_zoom_step;
    app_state->workspace_authoring_theme_preset_id = runtime->workspace_authoring_theme_preset_id;
    app_state->workspace_authoring_custom_theme_active_slot =
        runtime->workspace_authoring_custom_theme_active_slot;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        app_state->workspace_authoring_custom_theme_slots[i] =
            runtime->workspace_authoring_custom_theme_slots[i];
        (void)snprintf(app_state->workspace_authoring_custom_theme_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "%s",
                       runtime->workspace_authoring_custom_theme_slot_names[i]);
    }
    app_state->workspace_authoring_custom_theme = runtime->workspace_authoring_custom_theme;
    app_state->workspace_authoring_entry_text_zoom_step = app_state->text_zoom_step;
    app_state->workspace_authoring_entry_theme_preset_id = app_state->workspace_authoring_theme_preset_id;
    app_state->workspace_authoring_entry_custom_theme_active_slot =
        app_state->workspace_authoring_custom_theme_active_slot;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        app_state->workspace_authoring_entry_custom_theme_slots[i] =
            app_state->workspace_authoring_custom_theme_slots[i];
        (void)snprintf(app_state->workspace_authoring_entry_custom_theme_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "%s",
                       app_state->workspace_authoring_custom_theme_slot_names[i]);
    }
    snprintf(app_state->input_root, sizeof(app_state->input_root), "%s", runtime->input_root);
    app_state->recent_input_root_count = runtime->recent_input_root_count;
    for (i = 0; i < (int)runtime->recent_input_root_count; ++i) {
        snprintf(app_state->recent_input_roots[i],
                 DATALAB_APP_PATH_CAP,
                 "%s",
                 runtime->recent_input_roots[i]);
    }
    app_state->recent_input_root_dropdown_open = 0;
    app_state->open_picker_requested = 0;
    app_state->panel_rescan_requested = panel_rescan_requested;
    app_state->playback_active = runtime->playback_active;
    app_state->playback_mode = runtime->playback_mode;
    app_state->playback_direction = runtime->playback_direction ? runtime->playback_direction : 1;
    app_state->playback_speed_index = datalab_playback_speed_index_clamp(runtime->playback_speed_index);
    app_state->playback_interval_ms =
        runtime->playback_interval_ms
            ? runtime->playback_interval_ms
            : datalab_playback_interval_for_speed_index(app_state->playback_speed_index);
    app_state->playback_last_advance_ticks = SDL_GetTicks();
    app_state->session_hud_collapsed = runtime->session_hud_collapsed;
    app_state->raster_viewport = runtime->raster_viewport;
    app_state->raster_viewport.drag_active = 0;
}

void datalab_runtime_copy_from_app_state(DatalabAppRuntime *runtime,
                                         const DatalabAppState *app_state) {
    int i = 0;
    if (!runtime || !app_state) {
        return;
    }
    runtime->workspace_authoring_theme_preset_id = app_state->workspace_authoring_theme_preset_id;
    runtime->workspace_authoring_custom_theme_active_slot =
        app_state->workspace_authoring_custom_theme_active_slot;
    runtime->workspace_authoring_custom_theme = app_state->workspace_authoring_custom_theme;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        runtime->workspace_authoring_custom_theme_slots[i] =
            app_state->workspace_authoring_custom_theme_slots[i];
        (void)snprintf(runtime->workspace_authoring_custom_theme_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "%s",
                       app_state->workspace_authoring_custom_theme_slot_names[i]);
    }
    runtime->text_zoom_step = app_state->text_zoom_step;
    snprintf(runtime->input_root, sizeof(runtime->input_root), "%s", app_state->input_root);
    datalab_normalize_input_root_path(runtime->input_root, sizeof(runtime->input_root));
    runtime->recent_input_root_count = app_state->recent_input_root_count;
    for (i = 0; i < (int)app_state->recent_input_root_count; ++i) {
        snprintf(runtime->recent_input_roots[i],
                 DATALAB_APP_PATH_CAP,
                 "%s",
                 app_state->recent_input_roots[i]);
    }
    runtime->playback_active = app_state->playback_active;
    runtime->playback_mode = app_state->playback_mode;
    runtime->playback_direction = app_state->playback_direction ? app_state->playback_direction : 1;
    runtime->playback_speed_index = datalab_playback_speed_index_clamp(app_state->playback_speed_index);
    runtime->playback_interval_ms = app_state->playback_interval_ms;
    runtime->session_hud_collapsed = app_state->session_hud_collapsed;
    runtime->raster_viewport = app_state->raster_viewport;
    runtime->raster_viewport.drag_active = 0;
}
