#include "app/datalab_runtime_pack.h"

#include <stdio.h>

#include "core_data.h"
#include "data/dataset_builders.h"

int datalab_runtime_load_frame(DatalabAppRuntime *runtime) {
    CoreResult load_r;
    if (!runtime || !runtime->pack_path || runtime->pack_path[0] == '\0') {
        return 1;
    }
    load_r = datalab_load_pack(runtime->pack_path, &runtime->frame);
    if (load_r.code != CORE_OK) {
        fprintf(stderr, "datalab: failed to load pack: %s\n", load_r.message);
        return 2;
    }
    runtime->frame_loaded = 1;
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
    datalab_print_frame_summary(runtime->pack_path, &runtime->frame);
}
