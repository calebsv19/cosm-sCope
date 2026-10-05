#include "render/render_view_picker_text.h"
#include "render/render_view_picker_text_presentation.h"
#include "render/render_view_internal.h"
#include "render/render_view_ui_surface.h"
#include "render/render_view_picker_interaction.h"
#include "kit_ui_interaction_sdl.h"
#include "render/render_view_picker_composition.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

static KitUiInteractionControl control(uint32_t domain, uint64_t value) {
    for (uint32_t i=0;i<g_datalab_button_surface.count;++i)
        if (g_datalab_button_surface.keys[i].domain==domain && g_datalab_button_surface.keys[i].value==value)
            return g_datalab_button_surface.controls[i];
    assert(!"missing painted control"); return (KitUiInteractionControl){0};
}
static SDL_Event pointer(Uint32 type, KitRenderRect r, int scale) {
    SDL_Event e={0}; e.type=type; e.button.button=SDL_BUTTON_LEFT;
    e.button.x=(int)((r.x+r.width/2)/scale); e.button.y=(int)((r.y+r.height/2)/scale); return e;
}
static void click(SDL_Window *w, SDL_Renderer *r, DatalabAppState *s, uint32_t domain, uint64_t value, int scale) {
    KitUiInteractionControl c=control(domain,value);
    SDL_Event e=pointer(SDL_MOUSEBUTTONDOWN,c.bounds,scale);
    assert(datalab_workspace_authoring_route_interaction_event(w,r,&e,s));
    e.type=SDL_MOUSEBUTTONUP;
    assert(datalab_workspace_authoring_route_interaction_event(w,r,&e,s));
}
static void paint_font(SDL_Renderer *r, DatalabAppState *s, int scale) {
    DatalabAuthoringThemePalette p;
    datalab_overlay_theme_palette(datalab_overlay_selected_theme(s),&s->workspace_authoring_custom_theme,&p);
    datalab_overlay_draw_font_theme_takeover(r,s,&p,1440*scale,1000*scale);
    datalab_ui_surface_draw_focus(r,s);
}
static void picker_proof(SDL_Window *w, SDL_Renderer *r, int scale) {
    char roots[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP]={{0}};
    char files[4][DATALAB_APP_PATH_CAP]={{0}};
    snprintf(roots[0],sizeof(roots[0]),"fixture-a"); snprintf(roots[1],sizeof(roots[1]),"fixture-b");
    assert(mkdir(roots[0],0700)==0 || scale==2); assert(mkdir(roots[1],0700)==0 || scale==2);
    SDL_Surface *bmp=SDL_CreateRGBSurfaceWithFormat(0,2,2,32,SDL_PIXELFORMAT_RGBA32);
    assert(bmp && SDL_SaveBMP(bmp,"fixture-b/proof.bmp")==0); SDL_FreeSurface(bmp);
    snprintf(files[0],sizeof(files[0]),"fixture-b/proof.bmp");
    DatalabPickerRecentUiState recent={.list_rect={20*scale,40*scale,200*scale,60*scale},
        .item_rects={{20*scale,40*scale,200*scale,30*scale},{20*scale,70*scale,200*scale,30*scale}},.visible_count=2};
    DatalabPickerRecentFileUiState file_ui={.list_rect={300*scale,40*scale,200*scale,30*scale},
        .item_rects={{300*scale,40*scale,200*scale,30*scale}},.visible_count=1};
    KitUiSurface surface={0};
    datalab_picker_collect_buttons(&surface,1440*scale,1000*scale,&recent,roots,2,&file_ui,files,1);
    assert(surface.count==3);
    SDL_Event e=pointer(SDL_MOUSEBUTTONDOWN,surface.controls[1].bounds,scale);
    KitUiSurfaceKey action;
    assert(datalab_picker_route_buttons(&surface,w,r,&e,0,&action) && !action.domain);
    /* Reordering paths while held retains semantic ownership, not row position. */
    char swap[DATALAB_APP_PATH_CAP]; snprintf(swap,sizeof(swap),"%s",roots[0]);
    snprintf(roots[0],sizeof(roots[0]),"%s",roots[1]); snprintf(roots[1],sizeof(roots[1]),"%s",swap);
    datalab_picker_collect_buttons(&surface,1440*scale,1000*scale,&recent,roots,2,&file_ui,files,1);
    e=pointer(SDL_MOUSEBUTTONUP,surface.controls[0].bounds,scale);
    assert(datalab_picker_route_buttons(&surface,w,r,&e,0,&action));
    assert(action.domain==DATALAB_PICKER_ROOT && action.value==datalab_ui_path_key("fixture-b"));
    DatalabInputCatalog catalog; datalab_input_catalog_init(&catalog);
    DatalabCatalogView view; datalab_catalog_view_init(&view);
    char input[DATALAB_APP_PATH_CAP]="fixture-a",edit[DATALAB_APP_PATH_CAP]="",status[256]="",out[DATALAB_APP_PATH_CAP]="";
    size_t root_count=2; uint64_t total=0,count=0,selected=0; int reveal=0,editing=1,done=0;
    DatalabPickerActionHost host={&catalog,&view,input,edit,status,out,sizeof(input),sizeof(status),sizeof(out),
        roots,&root_count,files,1,"",&total,&count,&selected,&reveal,&editing,&done};
    datalab_picker_apply_button(&host,action);
    assert(strcmp(input,"fixture-b")==0 && count==1 && reveal && !editing && !done);
    e=pointer(SDL_MOUSEBUTTONDOWN,surface.controls[2].bounds,scale);
    assert(datalab_picker_route_buttons(&surface,w,r,&e,0,&action) && !action.domain);
    e.type=SDL_MOUSEBUTTONUP;
    assert(datalab_picker_route_buttons(&surface,w,r,&e,0,&action));
    datalab_picker_apply_button(&host,action);
    assert(done && strcmp(out,"fixture-b/proof.bmp")==0);
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_SPACE;
    assert(!datalab_picker_route_buttons(&surface,w,r,&e,1,&action));
    assert(!surface.interaction.focused_id);
    datalab_catalog_view_destroy(&view); datalab_input_catalog_destroy(&catalog);
}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1); assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Window *w=SDL_CreateWindow("interaction proof",0,0,1440,1000,SDL_WINDOW_HIDDEN); assert(w);
    for (int scale=1;scale<=2;++scale) {
        SDL_Surface *pixels=SDL_CreateRGBSurfaceWithFormat(0,1440*scale,1000*scale,32,SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer *r=SDL_CreateSoftwareRenderer(pixels); assert(r && pixels);
        DatalabAppState s; datalab_app_state_init(&s,"fixture.pack",DATALAB_PROFILE_IMAGE);
        datalab_ui_surface_reset(); datalab_workspace_authoring_begin_takeover(&s);
        s.workspace_authoring_overlay_mode=DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME;
        paint_font(r,&s,scale);
        KitUiInteractionControl inc=control(DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_TEXT_INC);
        int step=s.text_zoom_step;
        SDL_Event e=pointer(SDL_MOUSEBUTTONUP,inc.bounds,scale);
        datalab_workspace_authoring_route_interaction_event(w,r,&e,&s); assert(s.text_zoom_step==step);
        e.type=SDL_MOUSEBUTTONDOWN;
        assert(datalab_workspace_authoring_route_interaction_event(w,r,&e,&s)); assert(s.text_zoom_step==step);
        assert(g_datalab_button_surface.interaction.captured_id==inc.id);
        e.type=SDL_MOUSEBUTTONUP;
        assert(datalab_workspace_authoring_route_interaction_event(w,r,&e,&s)); assert(s.text_zoom_step==step+1);
        step=s.text_zoom_step;
        memset(&e,0,sizeof(e)); e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_SPACE;
        assert(datalab_workspace_authoring_route_interaction_event(w,r,&e,&s));
        e.key.repeat=1; datalab_workspace_authoring_route_interaction_event(w,r,&e,&s);
        e.type=SDL_KEYUP; e.key.repeat=0;
        assert(datalab_workspace_authoring_route_interaction_event(w,r,&e,&s)); assert(s.text_zoom_step==step+1);
        paint_font(r,&s,scale);
        /* Disabled custom font has no focus or activation. */
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_FONT_CUSTOM,scale);
        assert(!g_datalab_button_surface.interaction.focused_id);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_1,scale);
        assert(s.workspace_authoring_custom_theme_active_slot==1);
        paint_font(r,&s,scale);
        char previous[DATALAB_CUSTOM_THEME_NAME_CAP]; snprintf(previous,sizeof(previous),"%s",s.workspace_authoring_custom_theme_slot_names[1]);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_RENAME,scale);
        assert(strcmp(previous,s.workspace_authoring_custom_theme_slot_names[1])!=0);
        paint_font(r,&s,scale);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT,scale);
        assert(s.workspace_authoring_custom_theme_popup_open); paint_font(r,&s,scale);
        for(uint32_t i=0;i<g_datalab_button_surface.count;++i) assert(g_datalab_button_surface.keys[i].domain==DATALAB_UI_FONT);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0+3,scale);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_G,scale);
        assert(s.workspace_authoring_custom_theme_selected_token==3 && s.workspace_authoring_custom_theme_selected_channel==1);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_INC_1,scale);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_ASSIST,scale);
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_CUSTOM_POPUP_CLOSE,scale);
        assert(!s.workspace_authoring_custom_theme_popup_open); paint_font(r,&s,scale);
        KitUiSurfaceKey restored={0};
        assert(kit_ui_surface_key(&g_datalab_button_surface,g_datalab_button_surface.interaction.focused_id,&restored));
        assert(restored.domain==DATALAB_UI_FONT && restored.value==DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT);
        puts("DataLab actual custom modal restores semantic button identity");
        /* Focus marker is painted by SDL at the host's output scale. */
        click(w,r,&s,DATALAB_UI_FONT,DATALAB_AUTHORING_FONT_HIT_TEXT_RESET,scale);
        datalab_ui_surface_draw_focus(r,&s); SDL_RenderPresent(r);
        KitRenderRect marker; assert(kit_ui_interaction_focus_marker(&g_datalab_button_surface.interaction,
            g_datalab_button_surface.controls,g_datalab_button_surface.count,&marker));
        Uint8 red,green,blue,alpha;
        Uint32 pixel=*((Uint32*)((char*)pixels->pixels+(int)marker.y*pixels->pitch)+(int)marker.x+1);
        SDL_GetRGBA(pixel,pixels->format,&red,&green,&blue,&alpha); assert(red==80 && green==180 && blue==255);
        const char *capture_dir=getenv("DATALAB_UI_CAPTURE_DIR");
        if(capture_dir) {
            char path[1024]; snprintf(path,sizeof(path),"%s/focus-%dx.bmp",capture_dir,scale);
            assert(SDL_SaveBMP(pixels,path)==0);
            printf("SDL focus output scale=%d marker=%.1f,%.1f,%.1f,%.1f pass\n",scale,marker.x,marker.y,marker.width,marker.height);
        }
        e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_SPACE;
        datalab_workspace_authoring_route_interaction_event(w,r,&e,&s);
        s.workspace_authoring_custom_theme_popup_open=1;
        e.type=SDL_KEYUP; assert(datalab_workspace_authoring_route_interaction_event(w,r,&e,&s));
        assert(!g_datalab_button_surface.interaction.focused_id);
        s.workspace_authoring_custom_theme_popup_open=0; paint_font(r,&s,scale);
        click(w,r,&s,DATALAB_UI_TOP,KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE,scale);
        assert(s.workspace_authoring_overlay_mode==DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE);
        assert(datalab_ui_surface_collect(&s,1440*scale,1000*scale).code==CORE_OK);
        click(w,r,&s,DATALAB_UI_TOP,KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY,scale);
        assert(!s.workspace_authoring_stub_active);
        datalab_workspace_authoring_begin_takeover(&s);
        assert(datalab_ui_surface_collect(&s,1440*scale,1000*scale).code==CORE_OK);
        click(w,r,&s,DATALAB_UI_TOP,KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL,scale);
        assert(!s.workspace_authoring_stub_active);
        datalab_draw_recent_input_root_header(r,&s); datalab_draw_playback_hud(r,&s);
        int speed=s.playback_speed_index;
        click(w,r,&s,DATALAB_UI_PLAYBACK,5u,scale); assert(s.playback_speed_index==speed+1);
        click(w,r,&s,DATALAB_UI_PLAYBACK,7u,scale); assert(s.playback_mode==DATALAB_PLAYBACK_MODE_BOUNCE);
        click(w,r,&s,DATALAB_UI_PLAYBACK,1u,scale); assert(!g_datalab_button_surface.interaction.focused_id);
        s.recent_input_root_count=1; snprintf(s.recent_input_roots[0],sizeof(s.recent_input_roots[0]),"fixture-b");
        click(w,r,&s,DATALAB_UI_RECENT_HEADER,1u,scale); assert(s.recent_input_root_dropdown_open);
        datalab_draw_recent_input_root_header(r,&s); datalab_draw_playback_hud(r,&s);
        assert(g_datalab_button_surface.count==2);
        /* Fixture creation precedes the real Recent directory activation. */
        picker_proof(w,r,scale);
        click(w,r,&s,DATALAB_UI_RECENT_ROOT,datalab_ui_path_key("fixture-b"),scale);
        assert(strcmp(s.input_root,"fixture-b")==0);
        memset(&e,0,sizeof(e)); e.type=SDL_WINDOWEVENT; e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
        datalab_workspace_authoring_route_interaction_event(w,r,&e,&s);
        assert(!g_datalab_button_surface.interaction.focused_id);
        datalab_set_text_zoom_step(0);
        char field[128]="aéWz";KitUiTextEdit edit={0};KitUiTextPresentation view={0};
        assert(kit_ui_text_bind(&edit,field,sizeof(field),KIT_UI_TEXT_SINGLE_LINE).code==CORE_OK);
        assert(kit_ui_text_position(&edit,3,1).code==CORE_OK);
        assert(kit_ui_text_compose(&edit,"中W",1,1).code==CORE_OK);
        SDL_RenderSetScale(r,(float)scale,(float)scale);SDL_RenderSetClipRect(r,NULL);
        SDL_SetRenderDrawColor(r,20,20,20,255);SDL_RenderClear(r);
        SDL_Rect outer={10,10,400,200};SDL_RenderSetClipRect(r,&outer);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
        assert(datalab_picker_text_present(r,&edit,&view,(SDL_Rect){30,30,220,datalab_text_line_height(1)},1,
            (SDL_Color){240,240,240,255},(SDL_Color){80,180,255,255}).code==CORE_OK);
        SDL_Rect restored_clip;SDL_RenderGetClipRect(r,&restored_clip);assert(!memcmp(&restored_clip,&outer,sizeof(outer)));
        SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(r,&blend);assert(blend==SDL_BLENDMODE_NONE);
        assert(!strcmp(field,"aéWz") && !strcmp(view.display,"a中WWz") && view.rows[0].selection.width>0 && view.rows[0].preedit.width>0);
        SDL_RenderPresent(r);
        int cx=(int)((view.caret.x+.5f)*scale),cy=(int)((view.caret.y+2)*scale);
        pixel=*((Uint32*)((char*)pixels->pixels+cy*pixels->pitch)+cx);SDL_GetRGBA(pixel,pixels->format,&red,&green,&blue,&alpha);
        assert(red==80 && green==180 && blue==255);
        if(capture_dir){char path[1024];snprintf(path,sizeof(path),"%s/text-%dx.bmp",capture_dir,scale);assert(SDL_SaveBMP(pixels,path)==0);}
        assert(datalab_picker_text_hit(&edit,&view,view.rows[0].origin.x,view.rows[0].origin.y).code==CORE_OK && edit.cursor==0 && !edit.composition[0]);
        printf("DataLab actual SDL font presentation %dx: preedit, selection, caret pixels, hit and enclosing clip restoration pass\n",scale);
        SDL_DestroyRenderer(r); SDL_FreeSurface(pixels);
    }
    char edit_path[32]="aé😀z";KitUiTextEdit editor={0};KitUiSurface buttons={0};SDL_Event text_event={0};
    text_event.type=SDL_KEYDOWN;text_event.key.keysym.sym=SDLK_LEFT;
    assert(datalab_picker_text_event(&editor,&buttons,&text_event,edit_path,sizeof(edit_path)).consumed);
    text_event.key.keysym.sym=SDLK_BACKSPACE;datalab_picker_text_event(&editor,&buttons,&text_event,edit_path,sizeof(edit_path));assert(strcmp(edit_path,"aéz")==0);
    text_event=(SDL_Event){0};text_event.type=SDL_TEXTEDITING;strcpy(text_event.edit.text,"候補");text_event.edit.length=2;
    datalab_picker_text_event(&editor,&buttons,&text_event,edit_path,sizeof(edit_path));assert(strcmp(edit_path,"aéz")==0);
    text_event=(SDL_Event){0};text_event.type=SDL_KEYDOWN;text_event.key.keysym.sym=SDLK_RETURN;
    assert(!datalab_picker_text_event(&editor,&buttons,&text_event,edit_path,sizeof(edit_path)).submit);
    text_event=(SDL_Event){0};text_event.type=SDL_TEXTINPUT;strcpy(text_event.text.text,"中");
    datalab_picker_text_event(&editor,&buttons,&text_event,edit_path,sizeof(edit_path));assert(strcmp(edit_path,"aé中z")==0);
    text_event=(SDL_Event){0};text_event.type=SDL_KEYDOWN;text_event.key.keysym.sym=SDLK_RETURN;
    assert(datalab_picker_text_event(&editor,&buttons,&text_event,edit_path,sizeof(edit_path)).submit);
    text_event.key.repeat=1;assert(!datalab_picker_text_event(&editor,&buttons,&text_event,edit_path,sizeof(edit_path)).submit);
    puts("DataLab picker text adapter: UTF8, preedit isolation and nonrepeating host commit intent pass");
    KitPaneComposition panes;
    assert(datalab_picker_panes_build(&panes,(SDL_Rect){0,0,800,100},(SDL_Rect){0,100,200,500},(SDL_Rect){200,100,400,500},(SDL_Rect){600,100,200,500},800,600).code==CORE_OK);
    SDL_Rect overflow={150,90,100,600};datalab_picker_pane_clip(&panes,DATALAB_PICKER_PANE_BROWSER,&overflow);
    assert(overflow.x==150&&overflow.y==101&&overflow.w==49&&overflow.h==498);
    assert(kit_pane_composition_hit(&panes,200,200).id==DATALAB_PICKER_PANE_PREVIEW);
    puts("DataLab picker panes: stable identity, viewport clipping and shared paint/input bounds pass");
    SDL_Surface *clip_surface=SDL_CreateRGBSurfaceWithFormat(0,120,60,32,SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *clip_renderer=SDL_CreateSoftwareRenderer(clip_surface);assert(clip_surface&&clip_renderer);
    SDL_SetRenderDrawColor(clip_renderer,0,0,0,255);SDL_RenderClear(clip_renderer);
    SDL_Rect parent_clip={20,0,30,40},wide_clip={0,0,120,60};SDL_RenderSetClipRect(clip_renderer,&parent_clip);
    draw_text_5x7_clipped(clip_renderer,&wide_clip,0,0,"WWWWWWWW",1,255,0,0,255);
    SDL_Rect restored;SDL_RenderGetClipRect(clip_renderer,&restored);assert(!memcmp(&restored,&parent_clip,sizeof(restored)));
    SDL_RenderPresent(clip_renderer);unsigned drawn=0;
    for(int y=0;y<60;++y)for(int x=0;x<120;++x){Uint32 pixel;memcpy(&pixel,(char*)clip_surface->pixels+y*clip_surface->pitch+x*4,4);
        Uint8 red,g,b,a;SDL_GetRGBA(pixel,clip_surface->format,&red,&g,&b,&a);
        if(x<20||x>=50||y>=40)assert(red==0);else drawn+=red>0;}
    assert(drawn);SDL_DestroyRenderer(clip_renderer);SDL_FreeSurface(clip_surface);
    puts("DataLab legacy caption: enclosing pane clip is intersected, pixels contained and state restored");
    SDL_DestroyWindow(w); SDL_Quit();
    puts("DataLab surface replay: 1x/2x release, keyboard repeat, disabled controls, top HUD, custom popup, playback, Recent roots and picker IO pass");
}
