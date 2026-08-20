#ifndef DATALAB_RUNTIME_PACK_H
#define DATALAB_RUNTIME_PACK_H

#include "datalab/datalab_app_main.h"

int datalab_runtime_load_frame(DatalabAppRuntime *runtime);
int datalab_runtime_validate_loaded_physics_dataset(DatalabAppRuntime *runtime);
void datalab_runtime_print_loaded_frame_summary(const DatalabAppRuntime *runtime);
void datalab_runtime_note_active_raster_content(DatalabAppRuntime *runtime);
void datalab_runtime_reset_prefetch(DatalabAppRuntime *runtime);
void datalab_runtime_prefetch_neighbors(DatalabAppRuntime *runtime);
/* Resolve the compact W4 focus-window intents against the retained catalog
 * and submit at most the bounded async worker capacity. */
int datalab_runtime_focus_request(DatalabAppRuntime *runtime,
                                  const DatalabAppState *app_state,
                                  const char *selected_path);

#endif
