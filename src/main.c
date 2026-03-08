#include <stdio.h>
#include <string.h>

#include "app/app_state.h"
#include "core_data.h"
#include "data/dataset_builders.h"
#include "data/pack_loader.h"
#include "render/render_view.h"

static void print_usage(const char *argv0) {
    printf("usage: %s --pack /path/to/frame.pack [--no-gui]\n", argv0);
}

int main(int argc, char **argv) {
    const char *pack_path = NULL;
    int no_gui = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pack") == 0 && (i + 1) < argc) {
            pack_path = argv[++i];
        } else if (strcmp(argv[i], "--no-gui") == 0) {
            no_gui = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!pack_path) {
        print_usage(argv[0]);
        return 1;
    }

    DatalabFrame frame;
    CoreResult load_r = datalab_load_pack(pack_path, &frame);
    if (load_r.code != CORE_OK) {
        fprintf(stderr, "datalab: failed to load pack: %s\n", load_r.message);
        return 2;
    }

    datalab_print_frame_summary(pack_path, &frame);

    if (frame.profile == DATALAB_PROFILE_PHYSICS) {
        CoreDataset dataset;
        CoreResult ds_r = datalab_build_dataset_from_frame(&frame, &dataset);
        if (ds_r.code != CORE_OK) {
            fprintf(stderr, "datalab: dataset build failed: %s\n", ds_r.message);
            datalab_frame_free(&frame);
            return 3;
        }
        core_dataset_free(&dataset);
    }

    if (!no_gui) {
        DatalabAppState app_state;
        datalab_app_state_init(&app_state, pack_path, frame.profile);

        CoreResult run_r = datalab_render_run(&frame, &app_state);
        datalab_frame_free(&frame);
        if (run_r.code != CORE_OK) {
            fprintf(stderr, "datalab: render failed: %s\n", run_r.message);
            return 4;
        }
    } else {
        datalab_frame_free(&frame);
    }

    return 0;
}
