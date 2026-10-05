#include "render/render_view_picker_composition.h"
#include <math.h>
CoreResult datalab_picker_panes_build(KitPaneComposition *view,SDL_Rect top,SDL_Rect browser,SDL_Rect preview,SDL_Rect directories,int width,int height) {
    SDL_Rect rects[]={top,browser,preview,directories};KitPaneCompositionSpec specs[4];
    for(unsigned i=0;i<4;++i)specs[i]=(KitPaneCompositionSpec){i+1,{rects[i].x,rects[i].y,rects[i].w,rects[i].h},1,0,0,1};
    return kit_pane_composition_build(view,specs,4,(CorePaneRect){0,0,width,height});
}

void datalab_picker_pane_clip(const KitPaneComposition *view,CorePaneId id,SDL_Rect *rect) {
    const KitPaneCompositionEntry *p=kit_pane_composition_find(view,id);if(!p||!rect)return;
    CorePaneRect c=p->visible_content;
    SDL_Rect content={(int)ceilf(c.x),(int)ceilf(c.y),(int)fmaxf(0,floorf(c.x+c.width)-ceilf(c.x)),(int)fmaxf(0,floorf(c.y+c.height)-ceilf(c.y))};
    SDL_Rect clipped={0};(void)SDL_IntersectRect(rect,&content,&clipped);*rect=clipped;
}
