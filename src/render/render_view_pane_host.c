#include "kit_ui_window_sdl.h"
#include "render/render_view_pane_host.h"
static KitPaneHost host;
static SDL_Rect source_bounds,header_bounds;
void datalab_view_panes_reset(void){kit_pane_host_init(&host);source_bounds=(SDL_Rect){0};header_bounds=(SDL_Rect){0};}
static CorePaneRect rect(SDL_Rect r){return (CorePaneRect){r.x,r.y,r.w,r.h};}
CoreResult datalab_view_panes_build(KitPaneComposition *out,int w,int h,SDL_Rect source,SDL_Rect header,int visible){
    KitPaneCompositionSpec panes[]={{1,{0,0,w,h},0,0,0,1},
        {2,visible?rect(source):(CorePaneRect){0},0,0,0,visible},
        {3,rect(header),0,0,0,1}};
    return kit_pane_composition_build(out,panes,3,(CorePaneRect){0,0,w,h});
}
CoreResult datalab_view_header_slots(int w,int h,int button_width,int pad,KitPaneHeaderLayout *out){
    KitPaneHeaderAction recent={1,(float)button_width,1};
    return kit_pane_header_layout(out,(CorePaneRect){0,0,w,h},pad,pad,120,&recent,1);
}
static CoreResult sync(SDL_Renderer *renderer,const DatalabAppState *s){
    int w=0,h=0;if(datalab_renderer_backend_output_size(renderer,&w,&h)!=0)
        return (CoreResult){CORE_ERR_INVALID_ARG,"missing viewer viewport"};
    KitPaneComposition view;CoreResult r=datalab_view_panes_build(&view,w,h,source_bounds,header_bounds,!s->session_hud_collapsed);
    if(r.code==CORE_OK)r=kit_pane_host_sync(&host,&view,s->workspace_authoring_stub_active||s->recent_input_root_dropdown_open,0,0);
    return r;
}
CoreResult datalab_view_pane_begin(SDL_Renderer *renderer,const DatalabAppState *s,CorePaneId id,SDL_Rect bounds,KitPaneSdlClip *saved){
    if(id==2)source_bounds=bounds;else if(id==3)header_bounds=bounds;
    CoreResult r=sync(renderer,s);if(r.code!=CORE_OK)return r;
    return kit_pane_content_begin_sdl(renderer,kit_pane_composition_find(&host.view,id),saved);
}
CorePaneId datalab_view_pane_event(SDL_Window *window,SDL_Renderer *renderer,const SDL_Event *event,DatalabAppState *s){
    if(!event||!s)return 0;
    if(s->pane_layout_edit.active && (!s->workspace_authoring_stub_active ||
        s->workspace_authoring_overlay_mode!=DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE || s->workspace_authoring_custom_theme_popup_open))
        datalab_workspace_authoring_projection_drag_finish(s,0);
    if(kit_ui_window_event_invalidates_sdl(event)){
        kit_pane_host_cancel(&host,0,0);datalab_raster_viewport_end_drag(&s->raster_viewport);
        datalab_workspace_authoring_projection_drag_finish(s,0);return 0;
    }
    if(event->type==SDL_KEYDOWN&&event->key.keysym.sym==SDLK_ESCAPE && s->pane_layout_edit.active){
        datalab_workspace_authoring_projection_drag_finish(s,0);return 2;
    }
    if(sync(renderer,s).code!=CORE_OK)return 0;
    int x=0,y=0,wx=0,wy=0;KitPaneHostEventType type=KIT_PANE_HOST_POINTER_MOVE;
    if(event->type==SDL_MOUSEBUTTONDOWN||event->type==SDL_MOUSEBUTTONUP){wx=event->button.x;wy=event->button.y;
        type=event->type==SDL_MOUSEBUTTONDOWN?KIT_PANE_HOST_POINTER_DOWN:KIT_PANE_HOST_POINTER_UP;
    }else if(event->type==SDL_MOUSEMOTION){wx=event->motion.x;wy=event->motion.y;}
    else if(event->type==SDL_MOUSEWHEEL)SDL_GetMouseState(&wx,&wy);
    else return 0;
    if(!datalab_render_map_window_to_renderer_point(window,renderer,wx,wy,&x,&y))return 0;
    if(host.blocked)datalab_raster_viewport_end_drag(&s->raster_viewport);
    return kit_pane_host_pointer(&host,type,x,y,0,0);
}
