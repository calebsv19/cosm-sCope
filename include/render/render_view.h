#ifndef DATALAB_RENDER_VIEW_H
#define DATALAB_RENDER_VIEW_H

#include "app/app_state.h"
#include "core_base.h"
#include "data/pack_loader.h"

CoreResult datalab_render_run(const DatalabFrame *frame, DatalabAppState *app_state);
CoreResult datalab_render_pick_pack_path(const char *initial_input_root,
                                         char *io_input_root,
                                         size_t input_root_cap,
                                         int *io_text_zoom_step,
                                         int *out_enter_authoring,
                                         char *out_pack_path,
                                         size_t out_pack_path_cap);

#endif
