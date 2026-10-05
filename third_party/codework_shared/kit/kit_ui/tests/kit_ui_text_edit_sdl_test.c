#include "kit_ui_text_edit_sdl.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    char text[32]="abc";KitUiTextEdit e={0};kit_ui_text_bind(&e,text,sizeof(text),1);SDL_Event event={0};
    event.type=SDL_TEXTEDITING;strcpy(event.edit.text,"候補");event.edit.length=2;
    assert(kit_ui_text_event_sdl(&e,&event).consumed && strcmp(text,"abc")==0);
    event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;
    assert(!kit_ui_text_event_sdl(&e,&event).submit);
    event.key.keysym.sym=SDLK_ESCAPE;assert(!kit_ui_text_event_sdl(&e,&event).cancel && !e.composition[0]);
    assert(kit_ui_text_event_sdl(&e,&event).cancel);
    event.key.keysym.sym=SDLK_RETURN;assert(kit_ui_text_event_sdl(&e,&event).submit);
    event.key.repeat=1;assert(!kit_ui_text_event_sdl(&e,&event).submit);event.key.repeat=0;
    event.type=SDL_TEXTINPUT;strcpy(event.text.text,"é");assert(kit_ui_text_event_sdl(&e,&event).changed && strcmp(text,"abcé")==0);
    event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_LEFT;event.key.keysym.mod=KMOD_SHIFT;
    kit_ui_text_event_sdl(&e,&event);assert(e.cursor==3 && e.anchor==5);
    event.key.keysym.sym=SDLK_BACKSPACE;event.key.keysym.mod=0;kit_ui_text_event_sdl(&e,&event);assert(strcmp(text,"abc")==0);
    e.flags=0;event.key.keysym.sym=SDLK_RETURN;assert(kit_ui_text_event_sdl(&e,&event).changed && strcmp(text,"abc\n")==0);
    puts("SDL normalization, IME isolation, host intent and repeat policy pass");
}
