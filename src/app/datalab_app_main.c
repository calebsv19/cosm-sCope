#include "datalab/datalab_app_main.h"

#include <stdio.h>
#include <string.h>

#include "core_data.h"
#include "data/dataset_builders.h"
#include "render/render_view.h"

static void datalab_print_usage(const char *argv0) {
    printf("usage: %s --pack /path/to/frame.pack [--no-gui]\n", argv0);
}

void datalab_app_runtime_init(DatalabAppRuntime *runtime) {
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    datalab_frame_init(&runtime->frame);
}

int datalab_app_bootstrap(int argc, char **argv, DatalabAppRuntime *runtime) {
    if (!runtime) {
        return 1;
    }

    runtime->argv0 = (argv && argc > 0 && argv[0]) ? argv[0] : "datalab";
    runtime->pack_path = NULL;
    runtime->no_gui = 0;
    runtime->show_help = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pack") == 0 && (i + 1) < argc) {
            runtime->pack_path = argv[++i];
        } else if (strcmp(argv[i], "--no-gui") == 0) {
            runtime->no_gui = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            datalab_print_usage(runtime->argv0);
            runtime->show_help = 1;
            return 0;
        }
    }

    return 0;
}

int datalab_app_config_load(DatalabAppRuntime *runtime) {
    (void)runtime;
    return 0;
}

int datalab_app_state_seed(DatalabAppRuntime *runtime) {
    if (!runtime) {
        return 1;
    }

    if (!runtime->pack_path) {
        datalab_print_usage(runtime->argv0 ? runtime->argv0 : "datalab");
        return 1;
    }

    CoreResult load_r = datalab_load_pack(runtime->pack_path, &runtime->frame);
    if (load_r.code != CORE_OK) {
        fprintf(stderr, "datalab: failed to load pack: %s\n", load_r.message);
        return 2;
    }
    runtime->frame_loaded = 1;

    datalab_print_frame_summary(runtime->pack_path, &runtime->frame);

    if (runtime->frame.profile == DATALAB_PROFILE_PHYSICS) {
        CoreDataset dataset;
        CoreResult ds_r = datalab_build_dataset_from_frame(&runtime->frame, &dataset);
        if (ds_r.code != CORE_OK) {
            fprintf(stderr, "datalab: dataset build failed: %s\n", ds_r.message);
            return 3;
        }
        core_dataset_free(&dataset);
    }

    return 0;
}

int datalab_app_subsystems_init(DatalabAppRuntime *runtime, DatalabAppState *app_state) {
    if (!runtime) {
        return 1;
    }
    if (runtime->no_gui) {
        return 0;
    }
    if (!app_state) {
        return 1;
    }

    datalab_app_state_init(app_state, runtime->pack_path, runtime->frame.profile);
    return 0;
}

int datalab_runtime_start(DatalabAppRuntime *runtime, DatalabAppState *app_state) {
    if (!runtime) {
        return 1;
    }
    if (runtime->no_gui) {
        return 0;
    }

    CoreResult run_r = datalab_render_run(&runtime->frame, app_state);
    if (run_r.code != CORE_OK) {
        fprintf(stderr, "datalab: render failed: %s\n", run_r.message);
        return 4;
    }

    return 0;
}

int datalab_app_run_loop(DatalabAppRuntime *runtime) {
    DatalabAppState app_state;
    int rc = datalab_app_subsystems_init(runtime, &app_state);
    if (rc != 0) {
        return rc;
    }
    return datalab_runtime_start(runtime, &app_state);
}

void datalab_app_shutdown(DatalabAppRuntime *runtime) {
    if (!runtime) {
        return;
    }
    if (runtime->frame_loaded) {
        datalab_frame_free(&runtime->frame);
        runtime->frame_loaded = 0;
    }
}

int datalab_app_main(int argc, char **argv) {
    DatalabAppRuntime runtime;
    int rc = 0;

    datalab_app_runtime_init(&runtime);

    rc = datalab_app_bootstrap(argc, argv, &runtime);
    if (rc != 0) {
        goto done;
    }

    if (runtime.show_help) {
        rc = 0;
        goto done;
    }

    rc = datalab_app_config_load(&runtime);
    if (rc != 0) {
        goto done;
    }

    rc = datalab_app_state_seed(&runtime);
    if (rc != 0) {
        goto done;
    }

    rc = datalab_app_run_loop(&runtime);

done:
    datalab_app_shutdown(&runtime);
    return rc;
}

int datalab_app_main_legacy(int argc, char **argv) {
    const char *pack_path = NULL;
    int no_gui = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pack") == 0 && (i + 1) < argc) {
            pack_path = argv[++i];
        } else if (strcmp(argv[i], "--no-gui") == 0) {
            no_gui = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            datalab_print_usage(argv[0]);
            return 0;
        }
    }

    if (!pack_path) {
        datalab_print_usage(argv[0]);
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
