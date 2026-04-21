#include "render/render_view_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kit_workspace_authoring.h"

enum {
    DATALAB_AUTHORING_LAYOUT_MODE_RUNTIME = 0,
    DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING = 1,
    DATALAB_CUSTOM_THEME_TOKEN_COUNT = 9,
    DATALAB_CUSTOM_THEME_CHANNEL_COUNT = 3
};

typedef struct DatalabWorkspaceAuthoringActionContext {
    DatalabAppState *app_state;
} DatalabWorkspaceAuthoringActionContext;

typedef struct DatalabWorkspaceAuthoringDiagState {
    uint64_t action_calls;
    uint64_t action_failures;
    uint64_t entry_chord_matches;
    uint64_t trigger_suppressed;
} DatalabWorkspaceAuthoringDiagState;

static DatalabWorkspaceAuthoringDiagState g_datalab_authoring_diag = {0};

static int datalab_env_flag_enabled(const char *name) {
    const char *value = NULL;
    if (!name) {
        return 0;
    }
    value = getenv(name);
    if (!value || !value[0]) {
        return 0;
    }
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static int datalab_sx4_diag_enabled(void) {
    return datalab_env_flag_enabled("DATALAB_SX4_DIAG");
}

static uint32_t datalab_authoring_mod_bits_from_sdl(SDL_Keymod mods) {
    uint32_t flags = 0u;
    if ((mods & KMOD_SHIFT) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    }
    if ((mods & KMOD_ALT) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    }
    if ((mods & KMOD_CTRL) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    }
    if ((mods & KMOD_GUI) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    }
    return flags;
}

static KitWorkspaceAuthoringKey datalab_authoring_key_from_sdl(SDL_Keycode key) {
    switch (key) {
        case SDLK_TAB:
            return KIT_WORKSPACE_AUTHORING_KEY_TAB;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return KIT_WORKSPACE_AUTHORING_KEY_ENTER;
        case SDLK_ESCAPE:
            return KIT_WORKSPACE_AUTHORING_KEY_ESCAPE;
        case SDLK_h:
            return KIT_WORKSPACE_AUTHORING_KEY_H;
        case SDLK_v:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        case SDLK_x:
            return KIT_WORKSPACE_AUTHORING_KEY_X;
        case SDLK_BACKSPACE:
            return KIT_WORKSPACE_AUTHORING_KEY_BACKSPACE;
        case SDLK_r:
            return KIT_WORKSPACE_AUTHORING_KEY_R;
        case SDLK_0:
        case SDLK_KP_0:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_0;
        case SDLK_1:
        case SDLK_KP_1:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_1;
        case SDLK_2:
        case SDLK_KP_2:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_2;
        case SDLK_3:
        case SDLK_KP_3:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3;
        case SDLK_4:
        case SDLK_KP_4:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_4;
        case SDLK_5:
        case SDLK_KP_5:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_5;
        case SDLK_6:
        case SDLK_KP_6:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_6;
        case SDLK_c:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDLK_z:
            return KIT_WORKSPACE_AUTHORING_KEY_Z;
        default:
            return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

static int datalab_authoring_is_active(const DatalabAppState *app_state) {
    return (app_state && app_state->workspace_authoring_stub_active) ? 1 : 0;
}

static int datalab_authoring_layout_mode(const DatalabAppState *app_state) {
    return datalab_authoring_is_active(app_state)
               ? DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING
               : DATALAB_AUTHORING_LAYOUT_MODE_RUNTIME;
}

static uint8_t datalab_authoring_theme_preset_clamp(int value) {
    if (value < (int)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT) {
        return (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT;
    }
    if (value > (int)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        return (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    }
    return (uint8_t)value;
}

static int datalab_custom_theme_token_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_TOKEN_COUNT) {
        return DATALAB_CUSTOM_THEME_TOKEN_COUNT - 1;
    }
    return value;
}

static int datalab_custom_theme_channel_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_CHANNEL_COUNT) {
        return DATALAB_CUSTOM_THEME_CHANNEL_COUNT - 1;
    }
    return value;
}

static int datalab_custom_theme_slot_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return DATALAB_CUSTOM_THEME_SLOT_COUNT - 1;
    }
    return value;
}

static DatalabWorkspaceCustomTheme *datalab_custom_theme_active_slot_ptr(DatalabAppState *app_state) {
    int slot_index = 0;
    if (!app_state) {
        return NULL;
    }
    slot_index = datalab_custom_theme_slot_clamp((int)app_state->workspace_authoring_custom_theme_active_slot);
    app_state->workspace_authoring_custom_theme_active_slot = (uint8_t)slot_index;
    return &app_state->workspace_authoring_custom_theme_slots[slot_index];
}

static void datalab_custom_theme_sync_from_slot(DatalabAppState *app_state) {
    DatalabWorkspaceCustomTheme *slot_theme = datalab_custom_theme_active_slot_ptr(app_state);
    if (!slot_theme) {
        return;
    }
    app_state->workspace_authoring_custom_theme = *slot_theme;
}

static uint8_t *datalab_custom_theme_channel_ptr(DatalabWorkspaceCustomTheme *theme,
                                                 int token_index,
                                                 int channel_index) {
    uint8_t *token = NULL;
    if (!theme) {
        return NULL;
    }
    switch (datalab_custom_theme_token_clamp(token_index)) {
        case 0: token = &theme->clear_r; break;
        case 1: token = &theme->pane_fill_r; break;
        case 2: token = &theme->shell_fill_r; break;
        case 3: token = &theme->shell_border_r; break;
        case 4: token = &theme->text_primary_r; break;
        case 5: token = &theme->text_secondary_r; break;
        case 6: token = &theme->button_fill_r; break;
        case 7: token = &theme->button_hover_r; break;
        case 8:
        default:
            token = &theme->button_active_r;
            break;
    }
    return token + datalab_custom_theme_channel_clamp(channel_index);
}

static int datalab_workspace_authoring_custom_theme_adjust(DatalabAppState *app_state, int delta) {
    uint8_t *channel_ptr = NULL;
    DatalabWorkspaceCustomTheme *active_theme = NULL;
    int token_index = 0;
    int channel_index = 0;
    int value = 0;

    if (!app_state || delta == 0) {
        return 0;
    }

    token_index = datalab_custom_theme_token_clamp((int)app_state->workspace_authoring_custom_theme_selected_token);
    channel_index = datalab_custom_theme_channel_clamp((int)app_state->workspace_authoring_custom_theme_selected_channel);
    app_state->workspace_authoring_custom_theme_selected_token = (uint8_t)token_index;
    app_state->workspace_authoring_custom_theme_selected_channel = (uint8_t)channel_index;
    active_theme = datalab_custom_theme_active_slot_ptr(app_state);
    if (!active_theme) {
        return 0;
    }
    channel_ptr = datalab_custom_theme_channel_ptr(active_theme,
                                                   token_index,
                                                   channel_index);
    if (!channel_ptr) {
        return 0;
    }
    value = (int)(*channel_ptr) + delta;
    if (value < 0) {
        value = 0;
    } else if (value > 255) {
        value = 255;
    }
    if (value == (int)(*channel_ptr)) {
        return 0;
    }
    *channel_ptr = (uint8_t)value;
    app_state->workspace_authoring_custom_theme = *active_theme;
    app_state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    app_state->workspace_authoring_pending_stub = 1u;
    return 1;
}

static int datalab_workspace_authoring_custom_theme_assist(DatalabAppState *app_state) {
    DatalabWorkspaceCustomTheme *active_theme = NULL;
    uint8_t *base = NULL;
    DatalabWorkspaceCustomTheme t = {0};
    int token_index = 0;
    float base_luma = 0.0f;
    int dark_mode = 0;

    if (!app_state) {
        return 0;
    }
    active_theme = datalab_custom_theme_active_slot_ptr(app_state);
    if (!active_theme) {
        return 0;
    }
    token_index = datalab_custom_theme_token_clamp((int)app_state->workspace_authoring_custom_theme_selected_token);
    base = datalab_custom_theme_channel_ptr(active_theme, token_index, 0);
    if (!base) {
        return 0;
    }

    base_luma = (0.2126f * (float)base[0]) + (0.7152f * (float)base[1]) + (0.0722f * (float)base[2]);
    dark_mode = base_luma < 122.0f;
    if (dark_mode) {
        t = (DatalabWorkspaceCustomTheme){
            (uint8_t)((base[0] * 12) / 100), (uint8_t)((base[1] * 12) / 100), (uint8_t)((base[2] * 12) / 100),
            (uint8_t)((base[0] * 36) / 100), (uint8_t)((base[1] * 36) / 100), (uint8_t)((base[2] * 36) / 100),
            (uint8_t)((base[0] * 24) / 100), (uint8_t)((base[1] * 24) / 100), (uint8_t)((base[2] * 24) / 100),
            (uint8_t)(((base[0] * 58) / 100) + 52), (uint8_t)(((base[1] * 58) / 100) + 52), (uint8_t)(((base[2] * 58) / 100) + 52),
            (uint8_t)(((base[0] * 12) / 100) + 210), (uint8_t)(((base[1] * 12) / 100) + 210), (uint8_t)(((base[2] * 12) / 100) + 210),
            (uint8_t)(((base[0] * 20) / 100) + 166), (uint8_t)(((base[1] * 20) / 100) + 166), (uint8_t)(((base[2] * 20) / 100) + 166),
            (uint8_t)((base[0] * 46) / 100), (uint8_t)((base[1] * 46) / 100), (uint8_t)((base[2] * 46) / 100),
            (uint8_t)((base[0] * 58) / 100), (uint8_t)((base[1] * 58) / 100), (uint8_t)((base[2] * 58) / 100),
            (uint8_t)((base[0] * 74) / 100), (uint8_t)((base[1] * 74) / 100), (uint8_t)((base[2] * 74) / 100)
        };
    } else {
        t = (DatalabWorkspaceCustomTheme){
            (uint8_t)(((base[0] * 10) / 100) + 228), (uint8_t)(((base[1] * 10) / 100) + 228), (uint8_t)(((base[2] * 10) / 100) + 228),
            (uint8_t)(((base[0] * 24) / 100) + 188), (uint8_t)(((base[1] * 24) / 100) + 188), (uint8_t)(((base[2] * 24) / 100) + 188),
            (uint8_t)(((base[0] * 18) / 100) + 206), (uint8_t)(((base[1] * 18) / 100) + 206), (uint8_t)(((base[2] * 18) / 100) + 206),
            (uint8_t)((base[0] * 48) / 100), (uint8_t)((base[1] * 48) / 100), (uint8_t)((base[2] * 48) / 100),
            (uint8_t)((base[0] * 8) / 100), (uint8_t)((base[1] * 8) / 100), (uint8_t)((base[2] * 8) / 100),
            (uint8_t)((base[0] * 22) / 100), (uint8_t)((base[1] * 22) / 100), (uint8_t)((base[2] * 22) / 100),
            (uint8_t)(((base[0] * 34) / 100) + 158), (uint8_t)(((base[1] * 34) / 100) + 158), (uint8_t)(((base[2] * 34) / 100) + 158),
            (uint8_t)(((base[0] * 28) / 100) + 176), (uint8_t)(((base[1] * 28) / 100) + 176), (uint8_t)(((base[2] * 28) / 100) + 176),
            (uint8_t)(((base[0] * 22) / 100) + 194), (uint8_t)(((base[1] * 22) / 100) + 194), (uint8_t)(((base[2] * 22) / 100) + 194)
        };
    }

    *active_theme = t;
    app_state->workspace_authoring_custom_theme = *active_theme;
    app_state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    app_state->workspace_authoring_pending_stub = 1u;
    return 1;
}

static void datalab_workspace_authoring_capture_entry_baseline(DatalabAppState *app_state) {
    int slot_index = 0;
    int selected_token = 0;
    int selected_channel = 0;
    if (!app_state) {
        return;
    }
    slot_index = datalab_custom_theme_slot_clamp((int)app_state->workspace_authoring_custom_theme_active_slot);
    app_state->workspace_authoring_custom_theme_active_slot = (uint8_t)slot_index;
    datalab_custom_theme_sync_from_slot(app_state);
    selected_token = datalab_custom_theme_token_clamp(
        (int)app_state->workspace_authoring_custom_theme_selected_token);
    selected_channel = datalab_custom_theme_channel_clamp(
        (int)app_state->workspace_authoring_custom_theme_selected_channel);
    app_state->workspace_authoring_custom_theme_selected_token = (uint8_t)selected_token;
    app_state->workspace_authoring_custom_theme_selected_channel = (uint8_t)selected_channel;
    app_state->workspace_authoring_entry_text_zoom_step = app_state->text_zoom_step;
    app_state->workspace_authoring_entry_theme_preset_id =
        datalab_authoring_theme_preset_clamp((int)app_state->workspace_authoring_theme_preset_id);
    app_state->workspace_authoring_entry_custom_theme = app_state->workspace_authoring_custom_theme;
    app_state->workspace_authoring_entry_custom_theme_active_slot = (uint8_t)slot_index;
    memcpy(app_state->workspace_authoring_entry_custom_theme_slots,
           app_state->workspace_authoring_custom_theme_slots,
           sizeof(app_state->workspace_authoring_entry_custom_theme_slots));
    memcpy(app_state->workspace_authoring_entry_custom_theme_slot_names,
           app_state->workspace_authoring_custom_theme_slot_names,
           sizeof(app_state->workspace_authoring_entry_custom_theme_slot_names));
    app_state->workspace_authoring_entry_custom_theme_selected_token = (uint8_t)selected_token;
    app_state->workspace_authoring_entry_custom_theme_selected_channel = (uint8_t)selected_channel;
}

static int datalab_authoring_reserved_key(KitWorkspaceAuthoringKey key) {
    switch (key) {
        case KIT_WORKSPACE_AUTHORING_KEY_H:
        case KIT_WORKSPACE_AUTHORING_KEY_V:
        case KIT_WORKSPACE_AUTHORING_KEY_X:
        case KIT_WORKSPACE_AUTHORING_KEY_BACKSPACE:
        case KIT_WORKSPACE_AUTHORING_KEY_R:
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_0:
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_1:
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_2:
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3:
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_4:
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_5:
        case KIT_WORKSPACE_AUTHORING_KEY_DIGIT_6:
        case KIT_WORKSPACE_AUTHORING_KEY_C:
            return 1;
        default:
            return 0;
    }
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

static void datalab_workspace_authoring_apply(DatalabAppState *app_state) {
    if (!app_state) {
        return;
    }
    app_state->workspace_authoring_pending_stub = 0u;
    app_state->workspace_authoring_custom_theme_popup_open = 0u;
    datalab_workspace_authoring_capture_entry_baseline(app_state);
    app_state->workspace_authoring_apply_count += 1u;
}

static void datalab_workspace_authoring_cancel_and_exit(DatalabAppState *app_state) {
    if (!app_state) {
        return;
    }
    if (app_state->workspace_authoring_pending_stub) {
        app_state->text_zoom_step = datalab_text_zoom_step_clamp(app_state->workspace_authoring_entry_text_zoom_step);
        app_state->workspace_authoring_theme_preset_id =
            datalab_authoring_theme_preset_clamp((int)app_state->workspace_authoring_entry_theme_preset_id);
        app_state->workspace_authoring_custom_theme_active_slot =
            (uint8_t)datalab_custom_theme_slot_clamp(
                (int)app_state->workspace_authoring_entry_custom_theme_active_slot);
        memcpy(app_state->workspace_authoring_custom_theme_slots,
               app_state->workspace_authoring_entry_custom_theme_slots,
               sizeof(app_state->workspace_authoring_custom_theme_slots));
        memcpy(app_state->workspace_authoring_custom_theme_slot_names,
               app_state->workspace_authoring_entry_custom_theme_slot_names,
               sizeof(app_state->workspace_authoring_custom_theme_slot_names));
        app_state->workspace_authoring_custom_theme = app_state->workspace_authoring_entry_custom_theme;
        datalab_custom_theme_sync_from_slot(app_state);
        datalab_set_text_zoom_step(app_state->text_zoom_step);
    }
    app_state->workspace_authoring_custom_theme_selected_token =
        (uint8_t)datalab_custom_theme_token_clamp(
            (int)app_state->workspace_authoring_entry_custom_theme_selected_token);
    app_state->workspace_authoring_custom_theme_selected_channel =
        (uint8_t)datalab_custom_theme_channel_clamp(
            (int)app_state->workspace_authoring_entry_custom_theme_selected_channel);
    app_state->workspace_authoring_pending_stub = 0u;
    app_state->workspace_authoring_stub_active = 0;
    app_state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    app_state->workspace_authoring_custom_theme_popup_open = 0u;
    app_state->workspace_authoring_entry_chord_mask = 0u;
    app_state->workspace_authoring_cancel_count += 1u;
}

static void datalab_workspace_authoring_toggle_mode(DatalabAppState *app_state) {
    if (!app_state) {
        return;
    }
    if (app_state->workspace_authoring_stub_active) {
        datalab_workspace_authoring_cancel_and_exit(app_state);
        return;
    }
    app_state->workspace_authoring_stub_active = 1;
    app_state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    app_state->workspace_authoring_pending_stub = 0u;
    app_state->workspace_authoring_custom_theme_popup_open = 0u;
    datalab_workspace_authoring_capture_entry_baseline(app_state);
    app_state->workspace_authoring_entry_count += 1u;
    app_state->workspace_authoring_entry_chord_mask = 0u;
}

static CoreResult datalab_workspace_authoring_execute_action_callback(void *host_context,
                                                                      const char *action_id,
                                                                      uint32_t *io_selected_pane_id,
                                                                      int *io_pending_apply) {
    DatalabWorkspaceAuthoringActionContext *ctx = (DatalabWorkspaceAuthoringActionContext *)host_context;
    if (!ctx || !ctx->app_state || !action_id || !io_selected_pane_id || !io_pending_apply) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring action request" };
    }

    if (strcmp(action_id, "workspace.toggle_mode") == 0) {
        datalab_workspace_authoring_toggle_mode(ctx->app_state);
    } else if (strcmp(action_id, "workspace.cycle_overlay") == 0) {
        datalab_workspace_authoring_cycle_overlay(ctx->app_state);
    } else if (strcmp(action_id, "workspace.apply") == 0) {
        datalab_workspace_authoring_apply(ctx->app_state);
    } else if (strcmp(action_id, "workspace.cancel_or_quit") == 0) {
        datalab_workspace_authoring_cancel_and_exit(ctx->app_state);
    } else {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "unsupported datalab authoring action" };
    }

    *io_pending_apply = (int)ctx->app_state->workspace_authoring_pending_stub;
    return core_result_ok();
}

CoreResult datalab_workspace_authoring_dispatch_action(DatalabAppState *app_state, const char *action_id) {
    DatalabWorkspaceAuthoringActionContext ctx = {0};
    KitWorkspaceAuthoringActionHooks hooks = {0};
    uint32_t selected_stub = 0u;
    int pending_apply = 0;
    CoreResult result = core_result_ok();

    if (!app_state || !action_id) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring dispatch request" };
    }

    g_datalab_authoring_diag.action_calls += 1u;
    ctx.app_state = app_state;
    hooks.execute_action = datalab_workspace_authoring_execute_action_callback;

    result = kit_workspace_authoring_execute_action(&ctx,
                                                    &hooks,
                                                    action_id,
                                                    &selected_stub,
                                                    &pending_apply,
                                                    datalab_authoring_layout_mode(app_state),
                                                    DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING,
                                                    app_state->workspace_authoring_overlay_mode,
                                                    DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE);
    if (result.code != CORE_OK) {
        g_datalab_authoring_diag.action_failures += 1u;
    }
    if (datalab_sx4_diag_enabled()) {
        fprintf(stdout,
                "[sx4-datalab] action=%s code=%d auth=%d overlay=%d pending=%u totals(action=%" PRIu64
                " fail=%" PRIu64 " chord=%" PRIu64 " suppress=%" PRIu64 ")\n",
                action_id,
                result.code,
                app_state->workspace_authoring_stub_active,
                (int)app_state->workspace_authoring_overlay_mode,
                (unsigned int)app_state->workspace_authoring_pending_stub,
                g_datalab_authoring_diag.action_calls,
                g_datalab_authoring_diag.action_failures,
                g_datalab_authoring_diag.entry_chord_matches,
                g_datalab_authoring_diag.trigger_suppressed);
    }
    return result;
}

void datalab_workspace_authoring_route_keydown(const SDL_KeyboardEvent *key,
                                               DatalabAppState *app_state,
                                               DatalabWorkspaceAuthoringAdapterResult *outcome) {
    KitWorkspaceAuthoringKey authoring_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    uint32_t mod_bits = 0u;
    int key_c_down = 0;
    int key_v_down = 0;
    const uint8_t *keyboard = NULL;
    const char *trigger = NULL;
    const char *action_id = NULL;
    CoreResult result = core_result_ok();

    if (outcome) {
        memset(outcome, 0, sizeof(*outcome));
    }
    if (!key || !app_state) {
        return;
    }

    authoring_key = datalab_authoring_key_from_sdl(key->keysym.sym);
    mod_bits = datalab_authoring_mod_bits_from_sdl((SDL_Keymod)key->keysym.mod);
    keyboard = SDL_GetKeyboardState(NULL);
    if (keyboard) {
        key_c_down = (keyboard[SDL_SCANCODE_C] != 0) ? 1 : 0;
        key_v_down = (keyboard[SDL_SCANCODE_V] != 0) ? 1 : 0;
    }

    if ((mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) != 0u &&
        (authoring_key == KIT_WORKSPACE_AUTHORING_KEY_C ||
         authoring_key == KIT_WORKSPACE_AUTHORING_KEY_V)) {
        if (outcome) {
            outcome->consumed = 1u;
        }
    }

    if (kit_workspace_authoring_entry_chord_pressed(authoring_key, mod_bits, key_c_down, key_v_down)) {
        g_datalab_authoring_diag.entry_chord_matches += 1u;
        result = datalab_workspace_authoring_dispatch_action(app_state, "workspace.toggle_mode");
        if (result.code == CORE_OK && datalab_authoring_is_active(app_state) && outcome) {
            outcome->entered_authoring = 1u;
        }
        if (outcome) {
            outcome->consumed = 1u;
        }
        return;
    }

    if (!datalab_authoring_is_active(app_state)) {
        return;
    }

    if (app_state->workspace_authoring_custom_theme_popup_open &&
        authoring_key == KIT_WORKSPACE_AUTHORING_KEY_ESCAPE) {
        app_state->workspace_authoring_custom_theme_popup_open = 0u;
        if (outcome) {
            outcome->consumed = 1u;
        }
        return;
    }

    if (app_state->workspace_authoring_custom_theme_popup_open) {
        int selected_token = datalab_custom_theme_token_clamp(
            (int)app_state->workspace_authoring_custom_theme_selected_token);
        int selected_channel = datalab_custom_theme_channel_clamp(
            (int)app_state->workspace_authoring_custom_theme_selected_channel);
        int shift_step = ((key->keysym.mod & KMOD_SHIFT) != 0) ? 10 : 1;

        switch (key->keysym.sym) {
            case SDLK_UP:
                app_state->workspace_authoring_custom_theme_selected_token =
                    (uint8_t)datalab_custom_theme_token_clamp(selected_token - 1);
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_DOWN:
                app_state->workspace_authoring_custom_theme_selected_token =
                    (uint8_t)datalab_custom_theme_token_clamp(selected_token + 1);
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_LEFT:
                app_state->workspace_authoring_custom_theme_selected_channel =
                    (uint8_t)datalab_custom_theme_channel_clamp(selected_channel - 1);
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_RIGHT:
                app_state->workspace_authoring_custom_theme_selected_channel =
                    (uint8_t)datalab_custom_theme_channel_clamp(selected_channel + 1);
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_TAB:
                app_state->workspace_authoring_custom_theme_selected_channel =
                    (uint8_t)((selected_channel + 1) % DATALAB_CUSTOM_THEME_CHANNEL_COUNT);
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_r:
                app_state->workspace_authoring_custom_theme_selected_channel = 0u;
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_g:
                app_state->workspace_authoring_custom_theme_selected_channel = 1u;
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_b:
                app_state->workspace_authoring_custom_theme_selected_channel = 2u;
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_a:
                (void)datalab_workspace_authoring_custom_theme_assist(app_state);
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                (void)datalab_workspace_authoring_custom_theme_adjust(app_state, -shift_step);
                if (outcome) outcome->consumed = 1u;
                return;
            case SDLK_EQUALS:
            case SDLK_PLUS:
            case SDLK_KP_PLUS:
                (void)datalab_workspace_authoring_custom_theme_adjust(app_state, shift_step);
                if (outcome) outcome->consumed = 1u;
                return;
            default:
                break;
        }
    }

    trigger = kit_workspace_authoring_trigger_from_key(authoring_key, mod_bits);
    if (trigger) {
        if (strcmp(trigger, "tab") == 0) {
            action_id = "workspace.cycle_overlay";
        } else if (strcmp(trigger, "enter") == 0) {
            action_id = "workspace.apply";
        } else if (strcmp(trigger, "esc") == 0) {
            action_id = "workspace.cancel_or_quit";
        }
        if (action_id) {
            result = datalab_workspace_authoring_dispatch_action(app_state, action_id);
            (void)result;
        } else {
            g_datalab_authoring_diag.trigger_suppressed += 1u;
            if (datalab_sx4_diag_enabled()) {
                fprintf(stdout,
                        "[sx4-datalab] trigger suppressed while active key=%d mods=0x%x count=%" PRIu64 "\n",
                        (int)authoring_key,
                        (unsigned int)mod_bits,
                        g_datalab_authoring_diag.trigger_suppressed);
            }
        }
        if (outcome) {
            outcome->consumed = 1u;
        }
        return;
    }

    if (datalab_authoring_reserved_key(authoring_key)) {
        g_datalab_authoring_diag.trigger_suppressed += 1u;
        if (outcome) {
            outcome->consumed = 1u;
        }
    }
}
