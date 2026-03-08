#ifndef DATALAB_INPUT_H
#define DATALAB_INPUT_H

#include <SDL2/SDL.h>

#include "app/app_state.h"

void datalab_handle_keydown(const SDL_KeyboardEvent *key, DatalabAppState *state, int *quit);

#endif
