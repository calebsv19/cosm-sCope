#include "app/datalab_runtime_pack.h"
#include "app/datalab_runtime_prefs.h"
#include "app/datalab_viewer_session_prefs.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "app/datalab_input_catalog.h"
#include "app/datalab_async_decode.h"
#include "core_data.h"
#include "data/dataset_builders.h"
#include "data/input_file_loader.h"


void datalab_runtime_note_active_raster_content(DatalabAppRuntime *runtime) {
    if (!runtime || !runtime->frame.drawing_rgba) {
        return;
    }
    if (runtime->raster_content_generation != UINT64_MAX) {
        runtime->raster_content_generation += 1u;
    }
    if (runtime->raster_content_generation == 0u) {
        runtime->raster_content_generation = 1u;
    }
    runtime->frame.raster_content_generation = runtime->raster_content_generation;
}

static const char *datalab_runtime_path_basename(const char *path) {
    const char *base = NULL;
    if (!path || path[0] == '\0') {
        return "";
    }
    base = strrchr(path, '/');
    if (!base) {
        base = strrchr(path, '\\');
    }
    return base ? (base + 1) : path;
}

static void datalab_runtime_format_load_failure(char *out_error,
                                                size_t out_error_cap,
                                                const char *input_path,
                                                const char *detail) {
    const char *base_name = datalab_runtime_path_basename(input_path);
    if (!out_error || out_error_cap == 0u) {
        return;
    }
    (void)snprintf(out_error,
                   out_error_cap,
                   "input load failed: %s (input=%s)",
                   (detail && detail[0] != '\0') ? detail : "unsupported or invalid file",
                   (base_name && base_name[0] != '\0') ? base_name : "unknown");
}

static int datalab_runtime_split_parent_dir(const char *path, char *out_dir, size_t out_dir_cap) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!path || !out_dir || out_dir_cap == 0u) {
        return 0;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        out_dir[0] = '\0';
        return 0;
    }
    len = (size_t)(slash - path);
    if (len == 0u) {
        if (out_dir_cap < 2u) {
            return 0;
        }
        out_dir[0] = '/';
        out_dir[1] = '\0';
        return 1;
    }
    if (len >= out_dir_cap) {
        len = out_dir_cap - 1u;
    }
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return 1;
}

void datalab_runtime_reset_prefetch(DatalabAppRuntime *runtime) {
    if (runtime && runtime->image_residency) {
        datalab_image_residency_clear_cpu(runtime->image_residency);
    }
}

static int datalab_runtime_prefetch_take_hit(DatalabAppRuntime *runtime) {
    if (!runtime || !runtime->image_residency || !runtime->pack_path || runtime->pack_path[0] == '\0') {
        return 0;
    }
    if (datalab_image_residency_take_cpu(runtime->image_residency, runtime->pack_path, &runtime->frame)) {
        datalab_runtime_note_active_raster_content(runtime);
        datalab_image_residency_note_active(runtime->image_residency, runtime->pack_path, &runtime->frame);
        runtime->frame_loaded = 1;
        return 1;
    }
    return 0;
}

void datalab_runtime_prefetch_neighbors(DatalabAppRuntime *runtime) {
    /* W4 replaces the former render-thread synchronous neighbor decoder.
     * Compatibility callers retain this no-op; interactive selection uses
     * datalab_runtime_focus_request below. */
    (void)runtime;
}

int datalab_runtime_focus_request(DatalabAppRuntime *runtime,
                                  const DatalabAppState *app_state,
                                  const char *selected_path) {
    DatalabFocusWindowIntent intent;
    char root[DATALAB_APP_PATH_CAP];
    char name[DATALAB_APP_PATH_CAP];
    char path[DATALAB_APP_PATH_CAP];
    uint64_t selected_index = 0u;
    int direction = 1;
    uint32_t velocity = 1u;
    int selected_submitted = 0;
    if (!runtime || !app_state || !selected_path || !selected_path[0] ||
        !datalab_runtime_split_parent_dir(selected_path, root, sizeof(root)) ||
        !datalab_input_catalog_root_matches(&runtime->input_catalog, root) ||
        !datalab_input_catalog_find_index(&runtime->input_catalog,
                                          datalab_runtime_path_basename(selected_path),
                                          &selected_index)) return 0;
    if (app_state->playback_direction < 0 || app_state->panel_selection_delta < 0) direction = -1;
    if (app_state->playback_active) velocity = (uint32_t)(app_state->playback_speed_index + 1);
    datalab_focus_window_select(&runtime->focus_window,
                                runtime->input_catalog.generation,
                                (uint64_t)runtime->input_catalog.file_count,
                                selected_index,
                                direction,
                                velocity,
                                app_state->playback_active);
    while (datalab_focus_window_pop_intent(&runtime->focus_window, &intent)) {
        int accepted = 0;
        if (!datalab_input_catalog_name_copy(&runtime->input_catalog, intent.logical_index, name, sizeof(name)) ||
            !datalab_input_catalog_file_is_current(&runtime->input_catalog, root, name) ||
            !datalab_input_root_join_child_file(root, name, path, sizeof(path))) {
            datalab_focus_window_note_complete(&runtime->focus_window, &intent, 0);
            continue;
        }
        if (intent.kind == DATALAB_FOCUS_WINDOW_INTENT_SELECTED) {
            accepted = datalab_async_decode_request_selected(&runtime->async_decode, path);
            if (accepted) {
                datalab_focus_window_set_pending(&runtime->focus_window, intent.logical_index);
                selected_submitted = 1;
            }
        } else {
            accepted = datalab_async_decode_request_neighbor(&runtime->async_decode,
                                                              path,
                                                              datalab_async_decode_current_generation(&runtime->async_decode));
        }
        if (!accepted) datalab_focus_window_note_complete(&runtime->focus_window, &intent, 0);
    }
    return selected_submitted;
}

int datalab_runtime_load_frame(DatalabAppRuntime *runtime) {
    CoreResult load_r;
    if (!runtime || !runtime->pack_path || runtime->pack_path[0] == '\0') {
        return 1;
    }
    runtime->last_load_error[0] = '\0';
    if (datalab_runtime_prefetch_take_hit(runtime)) {
        return 0;
    }
    load_r = datalab_load_input_file(runtime->pack_path, &runtime->frame);
    if (load_r.code != CORE_OK) {
        datalab_runtime_format_load_failure(runtime->last_load_error,
                                            sizeof(runtime->last_load_error),
                                            runtime->pack_path,
                                            load_r.message);
        fprintf(stderr,
                "datalab: load failed stage=input_load code=%d input=%s detail=%s\n",
                (int)load_r.code,
                datalab_runtime_path_basename(runtime->pack_path),
                load_r.message ? load_r.message : "unsupported or invalid file");
        return 2;
    }
    runtime->frame_loaded = 1;
    datalab_runtime_note_active_raster_content(runtime);
    if (runtime->viewer_session_restore_pending) {
        datalab_viewer_session_apply_presentation(&runtime->viewer_session,
                                                  runtime->frame.profile,
                                                  runtime->frame.width,
                                                  runtime->frame.height,
                                                  &runtime->raster_viewport,
                                                  &runtime->playback_active,
                                                  &runtime->playback_mode,
                                                  &runtime->playback_speed_index,
                                                  &runtime->playback_interval_ms,
                                                  &runtime->session_hud_collapsed,
                                                  &runtime->sampling_mode);
        runtime->viewer_session_restore_pending = 0;
    }
    if (runtime->image_residency && runtime->frame.profile == DATALAB_PROFILE_IMAGE) {
        datalab_image_residency_note_active(runtime->image_residency, runtime->pack_path, &runtime->frame);
    }
    (void)datalab_runtime_prefs_save_last_opened_input_file(runtime->pack_path);
    return 0;
}

int datalab_runtime_validate_loaded_physics_dataset(DatalabAppRuntime *runtime) {
    CoreDataset dataset;
    CoreResult ds_r;
    if (!runtime || !runtime->frame_loaded) {
        return 1;
    }
    if (runtime->frame.profile != DATALAB_PROFILE_PHYSICS) {
        return 0;
    }
    ds_r = datalab_build_dataset_from_frame(&runtime->frame, &dataset);
    if (ds_r.code != CORE_OK) {
        fprintf(stderr, "datalab: dataset build failed: %s\n", ds_r.message);
        return 3;
    }
    core_dataset_free(&dataset);
    return 0;
}

void datalab_runtime_print_loaded_frame_summary(const DatalabAppRuntime *runtime) {
    if (!runtime || !runtime->pack_path || runtime->pack_path[0] == '\0' || !runtime->frame_loaded) {
        return;
    }
    if (runtime->frame.profile == DATALAB_PROFILE_IMAGE) {
        printf("input=%s\n", runtime->pack_path);
        printf("  profile=image raster=%ux%u\n",
               runtime->frame.width,
               runtime->frame.height);
        return;
    }
    datalab_print_frame_summary(runtime->pack_path, &runtime->frame);
}
