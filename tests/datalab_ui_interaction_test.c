#include "render/render_view_authoring_overlay_shared.h"
#include "kit_ui_interaction_sdl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Software SDL output exercises this host's real window-to-output mapping
     * without requiring a WindowServer or modifying user appearance prefs. */
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Window *window=SDL_CreateWindow("interaction proof",0,0,1440,1000,SDL_WINDOW_HIDDEN);
    assert(window);
    for (int scale=1; scale<=2; ++scale) {
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1440*scale,1000*scale,32,SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer *renderer=SDL_CreateSoftwareRenderer(surface);
        assert(surface && renderer);
        DatalabAppState state;
        datalab_app_state_init(&state,"fixture.pack",DATALAB_PROFILE_IMAGE);
        datalab_workspace_authoring_begin_takeover(&state);
        state.workspace_authoring_overlay_mode=DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME;
        kit_ui_interaction_reset(&g_datalab_authoring_overlay_ui.font_theme_interaction);
        KitWorkspaceAuthoringFontThemeLayout layout;
        assert(datalab_authoring_build_font_theme_layout(&state,1440*scale,1000*scale,&layout));
        SDL_Event event={0};
        event.type=SDL_MOUSEBUTTONUP; event.button.button=SDL_BUTTON_LEFT;
        event.button.x=(int)((layout.text_size_inc_button.x+8)/scale);
        event.button.y=(int)((layout.text_size_inc_button.y+8)/scale);
        int step=state.text_zoom_step;
        datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state);
        assert(state.text_zoom_step==step);
        event.type=SDL_MOUSEBUTTONDOWN;
        assert(datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        assert(state.text_zoom_step==step && g_datalab_authoring_overlay_ui.font_theme_interaction.captured_id==2u);
        event.type=SDL_MOUSEBUTTONUP;
        assert(datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        assert(state.text_zoom_step>step);
        step=state.text_zoom_step;
        memset(&event,0,sizeof(event)); event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_SPACE;
        assert(datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        event.key.repeat=1; datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state);
        event.type=SDL_KEYUP; event.key.repeat=0;
        assert(datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        assert(state.text_zoom_step==step+1);
        kit_ui_interaction_reset(&g_datalab_authoring_overlay_ui.font_theme_interaction);
        event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_TAB;
        assert(datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        assert(g_datalab_authoring_overlay_ui.font_theme_interaction.focused_id==4u);
        assert(datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        assert(g_datalab_authoring_overlay_ui.font_theme_interaction.focused_id==5u);
        assert(datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        assert(g_datalab_authoring_overlay_ui.font_theme_interaction.focused_id==1u);
        SDL_SetRenderDrawColor(renderer,0,0,0,255); SDL_RenderClear(renderer);
        datalab_workspace_authoring_draw_interaction_focus(renderer,&state,1440*scale,1000*scale);
        SDL_RenderPresent(renderer);
        assert(datalab_authoring_build_font_theme_layout(&state,1440*scale,1000*scale,&layout));
        int x=(int)roundf(layout.text_size_dec_button.x)+10;
        int y=(int)roundf(layout.text_size_dec_button.y)+(int)roundf(layout.text_size_dec_button.height)-3;
        Uint8 red,green,blue,alpha;
        Uint32 pixel=*((Uint32*)((char*)surface->pixels+y*surface->pitch)+x);
        SDL_GetRGBA(pixel,surface->format,&red,&green,&blue,&alpha);
        assert(red==80 && green==180 && blue==255);
        /* A popup must not inherit a held background action or focus marker. */
        event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_SPACE;
        datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state);
        state.workspace_authoring_custom_theme_popup_open=1;
        event.type=SDL_KEYUP;
        assert(!datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state));
        assert(!g_datalab_authoring_overlay_ui.font_theme_interaction.focused_id &&
               !g_datalab_authoring_overlay_ui.font_theme_interaction.key_owner_id);
        assert(state.text_zoom_step==step+1);
        state.workspace_authoring_custom_theme_popup_open=0;
        event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_TAB;
        datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state);
        event.type=SDL_WINDOWEVENT; event.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
        datalab_workspace_authoring_route_interaction_event(window,renderer,&event,&state);
        assert(!g_datalab_authoring_overlay_ui.font_theme_interaction.focused_id);
        SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface);
    }
    SDL_DestroyWindow(window); SDL_Quit();
    puts("DataLab interaction: real 1x/2x coordinate mapping, actions, release/repeat, focus pixels and modal cancellation pass");
    return 0;
}
