#ifndef DATALAB_PICKER_TEXT_H
#define DATALAB_PICKER_TEXT_H
#include "kit_ui_text_edit_sdl.h"
#include "kit_ui_surface.h"
/* Caller owns field buffers and commit/cancel policy. */
KitUiTextEventResult datalab_picker_text_event(KitUiTextEdit *edit,KitUiSurface *buttons,
    const SDL_Event *event,char *text,size_t capacity);
#endif
