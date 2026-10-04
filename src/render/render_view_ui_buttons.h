#ifndef DATALAB_RENDER_VIEW_UI_BUTTONS_H
#define DATALAB_RENDER_VIEW_UI_BUTTONS_H

#include "render/render_view_authoring_overlay_shared.h"
#include "kit_ui_sdl.h"

/* Synchronous drawing: labels need to live through the call. Product actions,
 * hit rectangles, persistence, and focus/capture remain with their existing host. */
void datalab_ui_draw_button(SDL_Renderer *renderer,
                            const SDL_Rect *rect,
                            const char *label,
                            const KitUiButtonState *state,
                            const DatalabAuthoringThemePalette *palette);
KitUiSdlTextApi datalab_ui_text_api(int clip_padding_x);

#endif
