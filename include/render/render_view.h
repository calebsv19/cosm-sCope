#ifndef DATALAB_RENDER_VIEW_H
#define DATALAB_RENDER_VIEW_H

#include "app/app_state.h"
#include "app/datalab_input_catalog.h"
#include "app/datalab_image_residency.h"
#include "core_base.h"
#include "data/pack_loader.h"

typedef struct DatalabRenderSession DatalabRenderSession;

CoreResult datalab_render_session_open(DatalabRenderSession **out_session);
void datalab_render_session_close(DatalabRenderSession *session);
CoreResult datalab_render_run_with_session(DatalabRenderSession *session,
                                           const DatalabFrame *frame,
                                           DatalabAppState *app_state);
CoreResult datalab_render_capture_first_frame(DatalabRenderSession *session,
                                              const DatalabFrame *frame,
                                              DatalabAppState *app_state,
                                              const char *output_path);
CoreResult datalab_render_run(const DatalabFrame *frame, DatalabAppState *app_state);
CoreResult datalab_render_pick_pack_path(DatalabInputCatalog *input_catalog,
                                         DatalabImageResidency *image_residency,
                                         const char *initial_input_root,
                                         const char *initial_status,
                                         char *io_input_root,
                                         size_t input_root_cap,
                                         int *io_text_zoom_step,
                                         uint8_t *io_theme_preset_id,
                                         DatalabWorkspaceCustomTheme *io_custom_theme,
                                         int *out_enter_authoring,
                                         char *out_pack_path,
                                         size_t out_pack_path_cap);

#endif
