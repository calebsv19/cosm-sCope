#include "render/render_view_picker_text.h"
#include "kit_ui_focus_scope.h"
KitUiTextEventResult datalab_picker_text_event(KitUiTextEdit *edit,KitUiSurface *buttons,
    const SDL_Event *event,char *text,size_t cap) {
    KitUiTextEventResult r={0,0,0,0,{CORE_OK,NULL}};
    if(!edit||!buttons||!event||!text)return r;
    r.status=kit_ui_text_bind(edit,text,cap,KIT_UI_TEXT_SINGLE_LINE);
    if(r.status.code!=CORE_OK) {r.consumed=1;return r;}
    r=kit_ui_text_event_sdl(edit,event);
    if(event->type==SDL_KEYDOWN && !(event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI|KMOD_ALT)) && event->key.keysym.sym>=32 && event->key.keysym.sym<127)r.consumed=1;
    if(r.consumed)kit_ui_focus_scope_text(buttons);
    return r;
}
