#include "render/render_view_ui_surface.h"
#include "kit_ui_interaction_sdl.h"
#include <math.h>
#include <stdio.h>
KitUiSurface g_datalab_button_surface;
static KitRenderRect viewport;
int datalab_ui_surface_viewport_matches(int w,int h) { return w==viewport.width && h==viewport.height; }
static uint32_t surface_scope(const DatalabAppState *s) {
    if (!s->workspace_authoring_stub_active) return s->recent_input_root_dropdown_open ? 2u : 1u;
    if (s->workspace_authoring_custom_theme_popup_open) return 5u;
    return s->workspace_authoring_overlay_mode==DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME ? 4u : 3u;
}
void datalab_ui_surface_reset(void) { kit_ui_surface_reset(&g_datalab_button_surface); }
uint64_t datalab_ui_path_key(const char *s) {
    uint64_t h=UINT64_C(14695981039346656037);
    for (;s && *s;++s) { h^=(unsigned char)*s; h*=UINT64_C(1099511628211); }
    return h;
}
void datalab_ui_surface_add(uint32_t domain, uint64_t value, SDL_Rect r, int enabled) {
    if (r.w<=0 || r.h<=0) return;
    KitUiInteractionControl control;
    (void)kit_ui_surface_register(&g_datalab_button_surface,(KitUiSurfaceKey){domain,value},
        (KitRenderRect){r.x,r.y,r.w,r.h},&viewport,enabled,&control);
}
CoreResult datalab_ui_surface_collect(const DatalabAppState *s, int w, int h) {
    if (!s || w<=0 || h<=0) return (CoreResult){CORE_ERR_INVALID_ARG,"invalid surface viewport"};
    viewport=(KitRenderRect){0,0,w,h};
    kit_ui_surface_begin(&g_datalab_button_surface,surface_scope(s));
    if (!s->workspace_authoring_stub_active) {
        datalab_recent_collect_buttons(s);
        if (!s->recent_input_root_dropdown_open) datalab_playback_hud_collect_buttons(s);
    } else if (!s->workspace_authoring_custom_theme_popup_open) {
        KitWorkspaceAuthoringOverlayButton top[3];
        uint32_t n=kit_workspace_authoring_ui_build_overlay_buttons(w,1,
            s->workspace_authoring_overlay_mode==DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE,top,3u);
        for (uint32_t i=0;i<n;++i) if (top[i].visible)
            datalab_ui_surface_add(DATALAB_UI_TOP,top[i].id,
                (SDL_Rect){top[i].rect.x,top[i].rect.y,top[i].rect.width,top[i].rect.height},1);
        if (s->workspace_authoring_overlay_mode==DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME) {
            KitWorkspaceAuthoringFontThemeLayout layout;
            if (datalab_authoring_build_font_theme_layout(s,w,h,&layout)) {
                KitUiInteractionControl controls[13];
                n=kit_workspace_authoring_font_theme_controls(&layout,controls,13u);
                for (uint32_t i=0;i<n;++i) {
                    KitRenderRect r=controls[i].bounds;
                    datalab_ui_surface_add(DATALAB_UI_FONT,
                        datalab_overlay_hit_from_shared_button(controls[i].id),
                        (SDL_Rect){roundf(r.x),roundf(r.y),roundf(r.width),roundf(r.height)},controls[i].enabled);
                }
            }
            if (datalab_ui_surface_viewport_matches(g_datalab_authoring_overlay_ui.viewport_w,
                g_datalab_authoring_overlay_ui.viewport_h)) {
            for (int i=0;i<DATALAB_CUSTOM_THEME_SLOT_COUNT;++i)
                datalab_ui_surface_add(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_0+i,
                    g_datalab_authoring_overlay_ui.custom_slot_buttons[i],1);
            datalab_ui_surface_add(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_RENAME,
                g_datalab_authoring_overlay_ui.custom_rename_button,1);
            }
        }
    } else if (datalab_ui_surface_viewport_matches(g_datalab_authoring_overlay_ui.viewport_w,
        g_datalab_authoring_overlay_ui.viewport_h)) {
        DatalabAuthoringOverlayUiState *u=&g_datalab_authoring_overlay_ui;
        datalab_ui_surface_add(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_POPUP_CLOSE,u->custom_popup_close_button,1);
        for (int i=0;i<DATALAB_CUSTOM_THEME_TOKEN_COUNT;++i)
            datalab_ui_surface_add(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0+i,u->custom_popup_token_rows[i],1);
        for (int i=0;i<DATALAB_CUSTOM_THEME_CHANNEL_COUNT;++i)
            datalab_ui_surface_add(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_R+i,u->custom_popup_channel_buttons[i],1);
        for (int i=0;i<4;++i)
            datalab_ui_surface_add(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_10+i,u->custom_popup_adjust_buttons[i],1);
        datalab_ui_surface_add(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_ASSIST,u->custom_popup_assist_button,1);
    }
    return kit_ui_surface_end(&g_datalab_button_surface);
}
KitUiButtonState datalab_ui_surface_button_state(uint32_t domain, uint64_t value,
    const SDL_Rect *r, int enabled, int selected) {
    KitUiInteractionControl c={0,{r->x,r->y,r->w,r->h},enabled};
    for (uint32_t i=0;i<g_datalab_button_surface.count;++i)
        if (g_datalab_button_surface.keys[i].domain==domain && g_datalab_button_surface.keys[i].value==value) {
            c.id=g_datalab_button_surface.controls[i].id; break;
        }
    return kit_ui_interaction_button_state(&g_datalab_button_surface.interaction,&c,selected);
}
void datalab_ui_surface_draw_button(SDL_Renderer *renderer, const SDL_Rect *r,
    const char *label, uint32_t domain, uint64_t value, int enabled, int selected,
    const DatalabAuthoringThemePalette *palette) {
    KitUiButtonState s=datalab_ui_surface_button_state(domain,value,r,enabled,selected);
    datalab_ui_draw_button(renderer,r,label,&s,palette);
}
void datalab_ui_surface_draw_focus(SDL_Renderer *renderer, const DatalabAppState *s) {
    int w=0,h=0;
    if (!renderer || SDL_GetRendererOutputSize(renderer,&w,&h)!=0) return;
    CoreResult result=datalab_ui_surface_collect(s,w,h);
    if (result.code!=CORE_OK) { fprintf(stderr,"datalab: surface collection failed: %s\n",result.message); return; }
    kit_ui_interaction_sdl_draw_focus(renderer,&g_datalab_button_surface.interaction,
        g_datalab_button_surface.controls,g_datalab_button_surface.count,(KitRenderColor){80,180,255,255});
}
