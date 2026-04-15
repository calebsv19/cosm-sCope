#include "render/render_view_internal.h"

#include <string.h>

enum {
    DATALAB_AUTHORING_CHORD_C = 1 << 0,
    DATALAB_AUTHORING_CHORD_V = 1 << 1
};

static int datalab_alt_modifier_active(SDL_Keymod mods) {
    return (mods & KMOD_ALT) != 0;
}

void datalab_workspace_authoring_route_keydown(const SDL_KeyboardEvent *key,
                                               DatalabAppState *app_state,
                                               DatalabWorkspaceAuthoringAdapterResult *outcome) {
    int alt_active = 0;
    if (outcome) {
        memset(outcome, 0, sizeof(*outcome));
    }
    if (!key || !app_state) {
        return;
    }

    alt_active = datalab_alt_modifier_active((SDL_Keymod)key->keysym.mod);
    if (!alt_active && app_state->workspace_authoring_entry_chord_mask != 0u) {
        app_state->workspace_authoring_entry_chord_mask = 0u;
    }
    if (!alt_active) {
        return;
    }

    if (key->keysym.sym == SDLK_c || key->keysym.sym == SDLK_v) {
        if (outcome) {
            outcome->consumed = 1u;
        }
        if (key->keysym.sym == SDLK_c) {
            app_state->workspace_authoring_entry_chord_mask |= DATALAB_AUTHORING_CHORD_C;
        } else {
            app_state->workspace_authoring_entry_chord_mask |= DATALAB_AUTHORING_CHORD_V;
        }
        if ((app_state->workspace_authoring_entry_chord_mask &
             (DATALAB_AUTHORING_CHORD_C | DATALAB_AUTHORING_CHORD_V)) ==
            (DATALAB_AUTHORING_CHORD_C | DATALAB_AUTHORING_CHORD_V)) {
            app_state->workspace_authoring_stub_active = 1;
            app_state->workspace_authoring_entry_count += 1u;
            app_state->workspace_authoring_entry_chord_mask = 0u;
            if (outcome) {
                outcome->entered_authoring = 1u;
            }
        }
        return;
    }

    if (app_state->workspace_authoring_entry_chord_mask != 0u) {
        app_state->workspace_authoring_entry_chord_mask = 0u;
    }
}
