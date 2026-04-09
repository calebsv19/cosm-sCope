#ifndef DATALAB_RUNTIME_PACK_H
#define DATALAB_RUNTIME_PACK_H

#include "datalab/datalab_app_main.h"

int datalab_runtime_load_frame(DatalabAppRuntime *runtime);
int datalab_runtime_validate_loaded_physics_dataset(DatalabAppRuntime *runtime);
void datalab_runtime_print_loaded_frame_summary(const DatalabAppRuntime *runtime);

#endif
