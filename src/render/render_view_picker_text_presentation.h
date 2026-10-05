#ifndef DATALAB_PICKER_TEXT_PRESENTATION_H
#define DATALAB_PICKER_TEXT_PRESENTATION_H
#include "render/render_view_internal.h"
#include "kit_ui_text_presentation_sdl.h"
CoreResult datalab_picker_text_present(SDL_Renderer *,KitUiTextEdit *,KitUiTextPresentation *,SDL_Rect,int,SDL_Color,SDL_Color);
CoreResult datalab_picker_text_hit(KitUiTextEdit *,const KitUiTextPresentation *,float,float);
#endif
