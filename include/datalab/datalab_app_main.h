#ifndef DATALAB_APP_MAIN_H
#define DATALAB_APP_MAIN_H

#include "app/app_state.h"
#include "data/pack_loader.h"

typedef struct DatalabAppRuntime {
    const char *argv0;
    const char *pack_path;
    int no_gui;
    int show_help;
    int text_zoom_step;
    int frame_loaded;
    int input_root_from_cli;
    char input_root[DATALAB_APP_PATH_CAP];
    char selected_pack_path[DATALAB_APP_PATH_CAP];
    DatalabFrame frame;
} DatalabAppRuntime;

void datalab_app_runtime_init(DatalabAppRuntime *runtime);

int datalab_app_main(int argc, char **argv);
int datalab_app_main_legacy(int argc, char **argv);

int datalab_app_bootstrap(int argc, char **argv, DatalabAppRuntime *runtime);
int datalab_app_config_load(DatalabAppRuntime *runtime);
int datalab_app_state_seed(DatalabAppRuntime *runtime);
int datalab_app_subsystems_init(DatalabAppRuntime *runtime, DatalabAppState *app_state);
int datalab_runtime_start(DatalabAppRuntime *runtime, DatalabAppState *app_state);
int datalab_app_run_loop(DatalabAppRuntime *runtime);
void datalab_app_shutdown(DatalabAppRuntime *runtime);

#endif
