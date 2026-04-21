#include "render/render_view_authoring_overlay_shared.h"

static int datalab_overlay_point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) {
        return 0;
    }
    return x >= rect->x && x < (rect->x + rect->w) && y >= rect->y && y < (rect->y + rect->h);
}

static DatalabAuthoringFontThemeHitId datalab_overlay_font_theme_hit_test(int x, int y) {
    int i;

    if (!g_datalab_authoring_overlay_ui.font_controls_valid) {
        return DATALAB_AUTHORING_FONT_HIT_NONE;
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.text_dec_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_TEXT_DEC;
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.text_inc_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_TEXT_INC;
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.text_reset_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_TEXT_RESET;
    }

    for (i = 0; i < DATALAB_FONT_THEME_THEME_BUTTON_COUNT; ++i) {
        if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.theme_buttons[i], x, y)) {
            return (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_THEME_0 + i);
        }
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_create_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_CUSTOM_CREATE;
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_edit_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT;
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_open_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT;
    }
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_slot_buttons[i], x, y)) {
            return (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_0 + i);
        }
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_rename_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_CUSTOM_RENAME;
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_popup_close_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_CUSTOM_POPUP_CLOSE;
    }
    for (i = 0; i < DATALAB_CUSTOM_THEME_TOKEN_COUNT; ++i) {
        if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_popup_token_rows[i], x, y)) {
            return (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0 + i);
        }
    }
    for (i = 0; i < DATALAB_CUSTOM_THEME_CHANNEL_COUNT; ++i) {
        if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_popup_channel_buttons[i], x, y)) {
            return (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_R + i);
        }
    }
    for (i = 0; i < 4; ++i) {
        if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_popup_adjust_buttons[i], x, y)) {
            return (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_10 + i);
        }
    }
    if (datalab_overlay_point_in_rect(&g_datalab_authoring_overlay_ui.custom_popup_assist_button, x, y)) {
        return DATALAB_AUTHORING_FONT_HIT_CUSTOM_ASSIST;
    }

    return DATALAB_AUTHORING_FONT_HIT_NONE;
}

static int datalab_overlay_hit_is_custom_popup_control(DatalabAuthoringFontThemeHitId hit_id) {
    if (hit_id == DATALAB_AUTHORING_FONT_HIT_CUSTOM_POPUP_CLOSE) {
        return 1;
    }
    if (hit_id >= DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0 &&
        hit_id <= DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_8) {
        return 1;
    }
    if (hit_id >= DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_R &&
        hit_id <= DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_B) {
        return 1;
    }
    if (hit_id >= DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_10 &&
        hit_id <= DATALAB_AUTHORING_FONT_HIT_CUSTOM_INC_10) {
        return 1;
    }
    if (hit_id == DATALAB_AUTHORING_FONT_HIT_CUSTOM_ASSIST) {
        return 1;
    }
    return 0;
}

static int datalab_overlay_apply_top_button(KitWorkspaceAuthoringOverlayButtonId button_id,
                                            DatalabAppState *app_state) {
    const char *action_id = NULL;

    if (!app_state) {
        return 0;
    }

    switch (button_id) {
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE:
            action_id = "workspace.cycle_overlay";
            break;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY:
            action_id = "workspace.apply";
            break;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL:
            action_id = "workspace.cancel_or_quit";
            break;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD:
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE:
        default:
            return 0;
    }

    (void)datalab_workspace_authoring_dispatch_action(app_state, action_id);
    return 1;
}

int datalab_workspace_authoring_route_mouse_event(const SDL_Event *event, DatalabAppState *app_state) {
    KitWorkspaceAuthoringOverlayButtonId top_button = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;
    DatalabAuthoringFontThemeHitId font_hit = DATALAB_AUTHORING_FONT_HIT_NONE;
    int pointer_x = 0;
    int pointer_y = 0;

    if (!event || !app_state || !app_state->workspace_authoring_stub_active) {
        return 0;
    }

    switch (event->type) {
        case SDL_MOUSEMOTION:
            pointer_x = event->motion.x;
            pointer_y = event->motion.y;
            top_button = kit_workspace_authoring_ui_overlay_hit_test(g_datalab_authoring_overlay_ui.top_buttons,
                                                                      g_datalab_authoring_overlay_ui.top_button_count,
                                                                      (float)pointer_x,
                                                                      (float)pointer_y);
            g_datalab_authoring_overlay_ui.hover_top_button = top_button;
            if (app_state->workspace_authoring_overlay_mode == DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME) {
                g_datalab_authoring_overlay_ui.hover_font_hit = datalab_overlay_font_theme_hit_test(pointer_x, pointer_y);
                if (app_state->workspace_authoring_custom_theme_popup_open &&
                    !datalab_overlay_hit_is_custom_popup_control(g_datalab_authoring_overlay_ui.hover_font_hit)) {
                    g_datalab_authoring_overlay_ui.hover_font_hit = DATALAB_AUTHORING_FONT_HIT_NONE;
                }
            } else {
                g_datalab_authoring_overlay_ui.hover_font_hit = DATALAB_AUTHORING_FONT_HIT_NONE;
            }
            return 1;

        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button != SDL_BUTTON_LEFT) {
                return 1;
            }
            pointer_x = event->button.x;
            pointer_y = event->button.y;
            top_button = kit_workspace_authoring_ui_overlay_hit_test(g_datalab_authoring_overlay_ui.top_buttons,
                                                                      g_datalab_authoring_overlay_ui.top_button_count,
                                                                      (float)pointer_x,
                                                                      (float)pointer_y);
            if (top_button != KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE) {
                g_datalab_authoring_overlay_ui.hover_top_button = top_button;
                (void)datalab_overlay_apply_top_button(top_button, app_state);
                return 1;
            }

            if (app_state->workspace_authoring_overlay_mode == DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME) {
                font_hit = datalab_overlay_font_theme_hit_test(pointer_x, pointer_y);
                if (app_state->workspace_authoring_custom_theme_popup_open &&
                    !datalab_overlay_hit_is_custom_popup_control(font_hit)) {
                    font_hit = DATALAB_AUTHORING_FONT_HIT_NONE;
                }
                g_datalab_authoring_overlay_ui.hover_font_hit = font_hit;
                if (font_hit != DATALAB_AUTHORING_FONT_HIT_NONE) {
                    (void)datalab_overlay_apply_font_theme_hit(font_hit, app_state);
                }
            }
            return 1;

        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            return 1;

        default:
            return 0;
    }
}
