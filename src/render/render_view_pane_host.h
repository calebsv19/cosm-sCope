#ifndef DATALAB_VIEW_PANE_HOST_H
#define DATALAB_VIEW_PANE_HOST_H
#include "render/render_view_internal.h"
#include "kit_pane_host.h"
#include "kit_pane_composition_sdl.h"
enum { DATALAB_VIEW_PANE_CANVAS=1, DATALAB_VIEW_PANE_SOURCES=2, DATALAB_VIEW_PANE_HEADER=3 };
void datalab_view_panes_reset(void);
CoreResult datalab_view_panes_build(KitPaneComposition *out,int w,int h,SDL_Rect sources,SDL_Rect header,int source_visible);
CoreResult datalab_view_pane_begin(SDL_Renderer *renderer,const DatalabAppState *state,CorePaneId id,SDL_Rect bounds,KitPaneSdlClip *saved);
CorePaneId datalab_view_pane_event(SDL_Window *window,SDL_Renderer *renderer,const SDL_Event *event,DatalabAppState *state);
CoreResult datalab_view_header_slots(int width,int height,int button_width,int padding,KitPaneHeaderLayout *out);
#endif
