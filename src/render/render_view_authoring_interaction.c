#include "render/render_view_ui_surface.h"
#include "kit_ui_interaction_sdl.h"

int datalab_workspace_authoring_route_interaction_event(SDL_Window *window, SDL_Renderer *renderer,
    const SDL_Event *event, DatalabAppState *state) {
    if (!window || !renderer || !event || !state) return 0;
    KitUiInteractionEvent normalized;
    if (!kit_ui_interaction_event_from_sdl(event,&normalized)) return 0;
    if(normalized.type==KIT_UI_INTERACTION_CANCEL)datalab_ui_surface_reset();
    int width=0,height=0;
    if (datalab_renderer_backend_output_size(renderer,&width,&height)!=0) return 0;
    /* Runtime rectangles are refreshed by painting. Resize cannot reuse an
     * old snapshot; preserve release ownership while invalidating its target. */
    if (event->type==SDL_WINDOWEVENT && (event->window.event==SDL_WINDOWEVENT_SIZE_CHANGED ||
        event->window.event==SDL_WINDOWEVENT_RESIZED)) {
        kit_ui_surface_begin(&g_datalab_button_surface,0u);
        (void)kit_ui_surface_end(&g_datalab_button_surface); return 0;
    }
    if (normalized.type>=KIT_UI_INTERACTION_POINTER_MOVE && normalized.type<=KIT_UI_INTERACTION_POINTER_UP) {
        int x,y;
        if (!datalab_render_map_window_to_renderer_point(window,renderer,(int)normalized.x,(int)normalized.y,&x,&y)) return 1;
        normalized.x=x; normalized.y=y;
    }
    if (datalab_ui_surface_collect(state,width,height).code!=CORE_OK) return 1;
    KitUiInteractionResult result;
    if (kit_ui_surface_route(&g_datalab_button_surface,&normalized,&result).code!=CORE_OK) return 1;
    if (result.activated_id && kit_ui_surface_take_activation(&g_datalab_button_surface,result.activated_id)) {
        KitUiSurfaceKey key;
        if (kit_ui_surface_key(&g_datalab_button_surface,result.activated_id,&key)) {
            if (key.domain==DATALAB_UI_TOP) datalab_overlay_apply_top_button(key.value,state);
            else if (key.domain==DATALAB_UI_FONT) datalab_overlay_apply_font_theme_hit(key.value,state);
            else if (key.domain==DATALAB_UI_PLAYBACK) datalab_playback_hud_apply_button(state,key.value);
            else datalab_recent_apply_button(state,key.domain,key.value);
        }
    }
    return normalized.type==KIT_UI_INTERACTION_CANCEL ? 0 : result.consumed;
}
void datalab_overlay_draw_font_theme_button(SDL_Renderer *renderer, const SDL_Rect *rect,
    const char *label, uint32_t id, int selected, const DatalabAuthoringThemePalette *palette) {
    datalab_ui_surface_draw_button(renderer,rect,label,DATALAB_UI_FONT,
        datalab_overlay_hit_from_shared_button(id),
        kit_workspace_authoring_ui_font_theme_button_enabled(id),selected,palette);
}
void datalab_workspace_authoring_draw_interaction_focus(SDL_Renderer *renderer, const DatalabAppState *state,
    int width, int height) {
    (void)width; (void)height; datalab_ui_surface_draw_focus(renderer,state);
}
