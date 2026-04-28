#include "app/datalab_runtime_pack.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core_data.h"
#include "data/dataset_builders.h"
#include "data/input_file_loader.h"

#define DATALAB_PREFETCH_SCAN_MAX 256

static const int k_prefetch_offsets[DATALAB_FRAME_PREFETCH_SLOT_COUNT] = { -1, 1, 2 };

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

static int datalab_runtime_name_cmp(const void *a, const void *b) {
    const char *aa = (const char *)a;
    const char *bb = (const char *)b;
    return strcasecmp(aa, bb);
}

static size_t datalab_runtime_scan_supported_files(const char *root,
                                                   char files[][DATALAB_APP_PATH_CAP],
                                                   size_t cap) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    size_t count = 0u;
    if (!root || root[0] == '\0' || !files || cap == 0u) {
        return 0u;
    }
    dir = opendir(root);
    if (!dir) {
        return 0u;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!datalab_input_file_is_supported(entry->d_name)) {
            continue;
        }
        if (count >= cap) {
            break;
        }
        snprintf(files[count], DATALAB_APP_PATH_CAP, "%s", entry->d_name);
        count++;
    }
    closedir(dir);
    if (count > 1u) {
        qsort(files, count, sizeof(files[0]), datalab_runtime_name_cmp);
    }
    return count;
}

static int datalab_runtime_find_name_index(const char *name,
                                           char files[][DATALAB_APP_PATH_CAP],
                                           size_t file_count) {
    size_t i = 0u;
    if (!name || !files) {
        return -1;
    }
    for (i = 0u; i < file_count; ++i) {
        if (strcasecmp(name, files[i]) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void datalab_runtime_prefetch_slot_clear(DatalabFramePrefetchSlot *slot) {
    if (!slot) {
        return;
    }
    if (slot->valid) {
        datalab_frame_free(&slot->frame);
        datalab_frame_init(&slot->frame);
    }
    slot->valid = 0;
    slot->path[0] = '\0';
}

static void datalab_runtime_prefetch_clear_all(DatalabAppRuntime *runtime) {
    int i = 0;
    if (!runtime) {
        return;
    }
    for (i = 0; i < DATALAB_FRAME_PREFETCH_SLOT_COUNT; ++i) {
        datalab_runtime_prefetch_slot_clear(&runtime->prefetch_slots[i]);
    }
}

void datalab_runtime_reset_prefetch(DatalabAppRuntime *runtime) {
    datalab_runtime_prefetch_clear_all(runtime);
}

static int datalab_runtime_prefetch_take_hit(DatalabAppRuntime *runtime) {
    int i = 0;
    if (!runtime || !runtime->pack_path || runtime->pack_path[0] == '\0') {
        return 0;
    }
    for (i = 0; i < DATALAB_FRAME_PREFETCH_SLOT_COUNT; ++i) {
        DatalabFramePrefetchSlot *slot = &runtime->prefetch_slots[i];
        if (!slot->valid) {
            continue;
        }
        if (strcasecmp(slot->path, runtime->pack_path) == 0) {
            runtime->frame = slot->frame;
            datalab_frame_init(&slot->frame);
            slot->valid = 0;
            slot->path[0] = '\0';
            runtime->frame_loaded = 1;
            return 1;
        }
    }
    return 0;
}

static void datalab_runtime_prefetch_neighbor_bmps(DatalabAppRuntime *runtime) {
    char root[DATALAB_APP_PATH_CAP];
    char files[DATALAB_PREFETCH_SCAN_MAX][DATALAB_APP_PATH_CAP];
    size_t file_count = 0u;
    const char *active_name = NULL;
    int active_index = -1;
    int slot_index = 0;
    if (!runtime || !runtime->pack_path || runtime->pack_path[0] == '\0') {
        return;
    }
    datalab_runtime_prefetch_clear_all(runtime);
    if (!datalab_input_file_is_bmp(runtime->pack_path)) {
        return;
    }
    if (!datalab_runtime_split_parent_dir(runtime->pack_path, root, sizeof(root))) {
        return;
    }
    file_count = datalab_runtime_scan_supported_files(root, files, DATALAB_PREFETCH_SCAN_MAX);
    if (file_count == 0u) {
        return;
    }
    active_name = datalab_runtime_path_basename(runtime->pack_path);
    active_index = datalab_runtime_find_name_index(active_name, files, file_count);
    if (active_index < 0) {
        return;
    }

    for (slot_index = 0; slot_index < DATALAB_FRAME_PREFETCH_SLOT_COUNT; ++slot_index) {
        int neighbor_index = active_index + k_prefetch_offsets[slot_index];
        DatalabFramePrefetchSlot *slot = &runtime->prefetch_slots[slot_index];
        CoreResult load_r;
        if (neighbor_index < 0 || (size_t)neighbor_index >= file_count) {
            continue;
        }
        if (!datalab_input_file_is_bmp(files[neighbor_index])) {
            continue;
        }
        snprintf(slot->path, sizeof(slot->path), "%s/%s", root, files[neighbor_index]);
        load_r = datalab_load_input_file(slot->path, &slot->frame);
        if (load_r.code != CORE_OK || slot->frame.profile != DATALAB_PROFILE_IMAGE) {
            slot->path[0] = '\0';
            datalab_frame_free(&slot->frame);
            datalab_frame_init(&slot->frame);
            continue;
        }
        slot->valid = 1;
    }
}

int datalab_runtime_load_frame(DatalabAppRuntime *runtime) {
    CoreResult load_r;
    if (!runtime || !runtime->pack_path || runtime->pack_path[0] == '\0') {
        return 1;
    }
    runtime->last_load_error[0] = '\0';
    if (datalab_runtime_prefetch_take_hit(runtime)) {
        datalab_runtime_prefetch_neighbor_bmps(runtime);
        return 0;
    }
    load_r = datalab_load_input_file(runtime->pack_path, &runtime->frame);
    if (load_r.code != CORE_OK) {
        fprintf(stderr, "datalab: failed to load input file: %s\n", load_r.message);
        snprintf(runtime->last_load_error,
                 sizeof(runtime->last_load_error),
                 "%s",
                 load_r.message ? load_r.message : "load failed");
        datalab_runtime_prefetch_clear_all(runtime);
        return 2;
    }
    runtime->frame_loaded = 1;
    datalab_runtime_prefetch_neighbor_bmps(runtime);
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
