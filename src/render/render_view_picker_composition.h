#ifndef RENDER_VIEW_PICKER_COMPOSITION_H
#define RENDER_VIEW_PICKER_COMPOSITION_H
#include "kit_pane_composition_sdl.h"
enum { DATALAB_PICKER_PANE_TOP=1,DATALAB_PICKER_PANE_BROWSER,DATALAB_PICKER_PANE_PREVIEW,DATALAB_PICKER_PANE_DIRECTORIES };
CoreResult datalab_picker_panes_build(KitPaneComposition *view,SDL_Rect top,SDL_Rect browser,SDL_Rect preview,SDL_Rect directories,int width,int height);
void datalab_picker_pane_clip(const KitPaneComposition *view,CorePaneId id,SDL_Rect *rect);
#endif
