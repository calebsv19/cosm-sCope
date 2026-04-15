#include "render/render_view_internal.h"

#include <string.h>

enum {
    DATALAB_AUTHORING_CHORD_C = 1 << 0,
    DATALAB_AUTHORING_CHORD_V = 1 << 1
};

static int datalab_alt_modifier_active(SDL_Keymod mods) {
    return (mods & KMOD_ALT) != 0;
}

static uint8_t datalab_authoring_chord_bit_from_scancode(SDL_Scancode scancode) {
    if (scancode == SDL_SCANCODE_C) {
        return DATALAB_AUTHORING_CHORD_C;
    }
    if (scancode == SDL_SCANCODE_V) {
        return DATALAB_AUTHORING_CHORD_V;
    }
    return 0u;
}

static void datalab_workspace_authoring_cycle_overlay(DatalabAppState *app_state) {
    if (!app_state) {
        return;
    }
    if (app_state->workspace_authoring_overlay_mode == DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE) {
        app_state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME;
    } else {
        app_state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    }
    app_state->workspace_authoring_pending_stub = 1u;
    app_state->workspace_authoring_overlay_cycle_count += 1u;
}

static void datalab_workspace_authoring_apply_stub(DatalabAppState *app_state) {
    if (!app_state) {
        return;
    }
    app_state->workspace_authoring_pending_stub = 0u;
    app_state->workspace_authoring_apply_count += 1u;
}

static void datalab_workspace_authoring_cancel_and_exit(DatalabAppState *app_state) {
    if (!app_state) {
        return;
    }
    app_state->workspace_authoring_pending_stub = 0u;
    app_state->workspace_authoring_stub_active = 0;
    app_state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    app_state->workspace_authoring_entry_chord_mask = 0u;
    app_state->workspace_authoring_cancel_count += 1u;
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

    if (app_state->workspace_authoring_stub_active) {
        switch (key->keysym.sym) {
            case SDLK_TAB:
                datalab_workspace_authoring_cycle_overlay(app_state);
                if (outcome) {
                    outcome->consumed = 1u;
                }
                return;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                datalab_workspace_authoring_apply_stub(app_state);
                if (outcome) {
                    outcome->consumed = 1u;
                }
                return;
            case SDLK_ESCAPE:
                datalab_workspace_authoring_cancel_and_exit(app_state);
                if (outcome) {
                    outcome->consumed = 1u;
                }
                return;
            default:
                break;
        }
    }

    alt_active = datalab_alt_modifier_active((SDL_Keymod)key->keysym.mod);
    if (!alt_active && app_state->workspace_authoring_entry_chord_mask != 0u) {
        app_state->workspace_authoring_entry_chord_mask = 0u;
    }
    if (!alt_active) {
        return;
    }

    {
        uint8_t chord_bit = datalab_authoring_chord_bit_from_scancode(key->keysym.scancode);
        if (chord_bit != 0u) {
            if (outcome) {
                outcome->consumed = 1u;
            }
            app_state->workspace_authoring_entry_chord_mask |= chord_bit;
            if ((app_state->workspace_authoring_entry_chord_mask &
                 (DATALAB_AUTHORING_CHORD_C | DATALAB_AUTHORING_CHORD_V)) ==
                (DATALAB_AUTHORING_CHORD_C | DATALAB_AUTHORING_CHORD_V)) {
                if (app_state->workspace_authoring_stub_active) {
                    datalab_workspace_authoring_cancel_and_exit(app_state);
                } else {
                    app_state->workspace_authoring_stub_active = 1;
                    app_state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
                    app_state->workspace_authoring_pending_stub = 0u;
                    app_state->workspace_authoring_entry_count += 1u;
                    app_state->workspace_authoring_entry_chord_mask = 0u;
                    if (outcome) {
                        outcome->entered_authoring = 1u;
                    }
                }
            }
            return;
        }
    }

    if (app_state->workspace_authoring_entry_chord_mask != 0u) {
        app_state->workspace_authoring_entry_chord_mask = 0u;
    }
}
