#include <stdio.h>

#include "core_data.h"
#include "data/dataset_builders.h"
#include "data/pack_loader.h"

int main(void) {
    DatalabFrame f = {0};
    f.profile = DATALAB_PROFILE_PHYSICS;

    f.width = 2;
    f.height = 2;
    f.frame_index = 42;
    f.time_seconds = 1.5;
    f.dt_seconds = 0.01;
    f.cell_size = 1.0f;

    float density[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float velx[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
    float vely[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
    f.density = density;
    f.velx = velx;
    f.vely = vely;

    CoreDataset ds;
    CoreResult r = datalab_build_dataset_from_frame(&f, &ds);
    if (r.code != CORE_OK) {
        fprintf(stderr, "smoke: dataset build failed: %s\n", r.message);
        return 1;
    }

    if (!core_dataset_find(&ds, "density") || !core_dataset_find(&ds, "velocity")) {
        core_dataset_free(&ds);
        fprintf(stderr, "smoke: expected fields missing\n");
        return 1;
    }

    core_dataset_free(&ds);
    puts("datalab smoke test passed");
    return 0;
}
