#include "render/render_view_authoring_overlay_shared.h"
#include "render/render_view_ui_buttons.h"
#include "kit_ui_interaction_sdl.h"
#include <math.h>

static int scope_active(const DatalabAppState *state) {
    return state && state->workspace_authoring_stub_active &&
        state->workspace_authoring_overlay_mode==DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME &&
        !state->workspace_authoring_custom_theme_popup_open;
}

static uint32_t controls_for(const DatalabAppState *state, int width, int height,
                              KitUiInteractionControl *controls) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    if (!datalab_authoring_build_font_theme_layout(state,width,height,&layout)) return 0u;
    uint32_t count=kit_workspace_authoring_font_theme_controls(&layout,controls,13u);
    /* Match the direct SDL button rectangles, which are snapped by this host. */
    for (uint32_t i=0; i<count; ++i) {
        KitRenderRect *r=&controls[i].bounds;
        r->x=roundf(r->x); r->y=roundf(r->y);
        r->width=roundf(r->width); r->height=roundf(r->height);
    }
    return count;
}

int datalab_workspace_authoring_route_interaction_event(SDL_Window *window, SDL_Renderer *renderer,
    const SDL_Event *event, DatalabAppState *state) {
    KitUiInteractionContext *ctx=&g_datalab_authoring_overlay_ui.font_theme_interaction;
    if (!scope_active(state)) { kit_ui_interaction_reset(ctx); return 0; }
    if (!window || !renderer || !event) return 0;
    KitUiInteractionEvent normalized;
    if (!kit_ui_interaction_event_from_sdl(event,&normalized)) return 0;
    int width=0,height=0;
    if (SDL_GetRendererOutputSize(renderer,&width,&height)!=0) { kit_ui_interaction_reset(ctx); return 0; }
    if (normalized.type>=KIT_UI_INTERACTION_POINTER_MOVE && normalized.type<=KIT_UI_INTERACTION_POINTER_UP) {
        int x,y;
        if (!datalab_render_map_window_to_renderer_point(window,renderer,(int)normalized.x,(int)normalized.y,&x,&y)) {
            kit_ui_interaction_reset(ctx); return 1;
        }
        normalized.x=(float)x; normalized.y=(float)y;
    }
    KitUiInteractionControl controls[13];
    uint32_t count=controls_for(state,width,height,controls);
    KitUiInteractionResult result;
    if (kit_ui_interaction_route(ctx,controls,count,&normalized,&result).code!=CORE_OK) return 1;
    g_datalab_authoring_overlay_ui.hover_font_hit=DATALAB_AUTHORING_FONT_HIT_NONE;
    for (uint32_t i=0; i<count; ++i) {
        if (kit_ui_interaction_button_state(ctx,&controls[i],0).hovered)
            g_datalab_authoring_overlay_ui.hover_font_hit=datalab_overlay_hit_from_shared_button(
                (KitWorkspaceAuthoringFontThemeButtonId)controls[i].id);
    }
    if (result.activated_id)
        datalab_overlay_apply_font_theme_hit(datalab_overlay_hit_from_shared_button(
            (KitWorkspaceAuthoringFontThemeButtonId)result.activated_id),state);
    if (!scope_active(state)) kit_ui_interaction_reset(ctx);
    /* The loop's lifecycle handling has already run; allow its remaining
     * routing for cancellation events, without dispatching another action. */
    return normalized.type==KIT_UI_INTERACTION_CANCEL ? 0 : result.consumed;
}

void datalab_overlay_draw_font_theme_button(SDL_Renderer *renderer, const SDL_Rect *rect,
    const char *label, uint32_t id, int selected, const DatalabAuthoringThemePalette *palette) {
    if (!rect) return;
    KitUiInteractionControl control={id,{(float)rect->x,(float)rect->y,(float)rect->w,(float)rect->h},
        kit_workspace_authoring_ui_font_theme_button_enabled((KitWorkspaceAuthoringFontThemeButtonId)id)};
    KitUiButtonState state=kit_ui_interaction_button_state(
        &g_datalab_authoring_overlay_ui.font_theme_interaction,&control,selected);
    datalab_ui_draw_button(renderer,rect,label,&state,palette);
}

void datalab_workspace_authoring_draw_interaction_focus(SDL_Renderer *renderer, const DatalabAppState *state,
    int width, int height) {
    if (!renderer || !scope_active(state)) return;
    KitUiInteractionControl controls[13];
    uint32_t count=controls_for(state,width,height,controls);
    kit_ui_interaction_sdl_draw_focus(renderer,&g_datalab_authoring_overlay_ui.font_theme_interaction,
        controls,count,(KitRenderColor){80,180,255,255});
}
