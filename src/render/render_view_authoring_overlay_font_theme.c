#include "render/render_view_authoring_overlay_shared.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void datalab_overlay_draw_centered_text(SDL_Renderer *renderer,
                                               const SDL_Rect *rect,
                                               int y,
                                               const char *text,
                                               uint8_t r,
                                               uint8_t g,
                                               uint8_t b,
                                               uint8_t a) {
    int text_w = 0;
    int text_h = 0;
    int x = 0;

    if (!renderer || !rect || !text) {
        return;
    }
    (void)datalab_measure_text(1, text, &text_w, &text_h);
    x = rect->x + ((rect->w - text_w) / 2);
    if (x < rect->x + datalab_scaled_px(6.0f)) {
        x = rect->x + datalab_scaled_px(6.0f);
    }
    draw_text_5x7(renderer, x, y, text, 1, r, g, b, a);
}

void datalab_overlay_draw_button(SDL_Renderer *renderer,
                                        const SDL_Rect *rect,
                                        const char *label,
                                        int hover,
                                        int active,
                                        const DatalabAuthoringThemePalette *palette) {
    int text_w = 0;
    int text_h = 0;
    int text_x = 0;
    int text_y = 0;

    if (!renderer || !rect || !label || !palette) {
        return;
    }

    if (active) {
        SDL_SetRenderDrawColor(renderer,
                               palette->button_active_r,
                               palette->button_active_g,
                               palette->button_active_b,
                               238);
    } else if (hover) {
        SDL_SetRenderDrawColor(renderer,
                               palette->button_hover_r,
                               palette->button_hover_g,
                               palette->button_hover_b,
                               234);
    } else {
        SDL_SetRenderDrawColor(renderer,
                               palette->button_fill_r,
                               palette->button_fill_g,
                               palette->button_fill_b,
                               228);
    }
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 242);
    SDL_RenderDrawRect(renderer, rect);

    (void)datalab_measure_text(1, label, &text_w, &text_h);
    text_x = rect->x + ((rect->w - text_w) / 2);
    text_y = rect->y + ((rect->h - text_h) / 2);
    if (text_x < rect->x + datalab_scaled_px(4.0f)) {
        text_x = rect->x + datalab_scaled_px(4.0f);
    }
    if (text_y < rect->y + datalab_scaled_px(2.0f)) {
        text_y = rect->y + datalab_scaled_px(2.0f);
    }
    draw_text_5x7(renderer,
                  text_x,
                  text_y,
                  label,
                  1,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);
}


void datalab_overlay_draw_font_theme_takeover(SDL_Renderer *renderer,
                                                     const DatalabAppState *app_state,
                                                     const DatalabAuthoringThemePalette *palette,
                                                     int ww,
                                                     int wh) {
    SDL_Rect panel = {0};
    SDL_Rect section = {0};
    SDL_Rect value_chip = {0};
    SDL_Rect custom_section = {0};
    SDL_Rect popup = {0};
    SDL_Rect popup_title = {0};
    int pad = 0;
    int row_h = 0;
    int button_h = 0;
    int button_small_w = 0;
    int button_reset_w = 0;
    int controls_gap = 0;
    int controls_x = 0;
    int controls_y = 0;
    int section_inner_x = 0;
    int section_inner_w = 0;
    int i = 0;
    int theme_step = 0;
    int zoom_percent = 100;
    int theme_button_w = 0;
    int theme_button_gap = 0;
    int theme_buttons_y = 0;
    int popup_row_h = 0;
    int popup_selected_token = 0;
    int popup_selected_channel = 0;
    int popup_channel_button_w = 0;
    int popup_adjust_button_w = 0;
    int popup_token_label_w = 0;
    int popup_row_start_y = 0;
    int popup_control_y = 0;
    int popup_inner_x = 0;
    int popup_inner_w = 0;
    int channel = 0;
    int custom_slot_gap = 0;
    int custom_slot_button_w = 0;
    int custom_slot_buttons_y = 0;
    int custom_action_gap = 0;
    int custom_action_y = 0;
    int custom_rename_w = 0;
    int custom_action_w = 0;
    int active_custom_slot = 0;
    char line[192];
    char size_line[80];
    char custom_line[160];
    char popup_row_line[96];
    DatalabWorkspaceAuthoringThemePreset selected_theme = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    const char *theme_name = NULL;
    static const char *k_theme_labels[DATALAB_FONT_THEME_THEME_BUTTON_COUNT] = {
        "daw_default",
        "standard_grey",
        "midnight_contrast",
        "soft_light",
        "greyscale",
        "custom"
    };
    static const char *k_custom_theme_tokens[DATALAB_CUSTOM_THEME_TOKEN_COUNT] = {
        "clear",
        "pane_fill",
        "shell_fill",
        "shell_border",
        "text_primary",
        "text_secondary",
        "button_fill",
        "button_hover",
        "button_active"
    };
    static const char *k_custom_theme_token_desc[DATALAB_CUSTOM_THEME_TOKEN_COUNT] = {
        "Window clear/background color",
        "Pane body fill color",
        "HUD/popup shell fill color",
        "HUD/popup border color",
        "Primary text color",
        "Secondary text color",
        "Button idle fill color",
        "Button hover fill color",
        "Button active fill color"
    };

    if (!renderer || !app_state || !palette || ww <= 0 || wh <= 0) {
        return;
    }

    selected_theme = datalab_overlay_selected_theme(app_state);
    theme_name = datalab_overlay_theme_name(selected_theme);
    active_custom_slot = datalab_overlay_custom_theme_slot_clamp(
        (int)app_state->workspace_authoring_custom_theme_active_slot);
    popup_selected_token = datalab_overlay_custom_theme_token_clamp(
        (int)app_state->workspace_authoring_custom_theme_selected_token);
    popup_selected_channel = datalab_overlay_custom_theme_channel_clamp(
        (int)app_state->workspace_authoring_custom_theme_selected_channel);
    memset(g_datalab_authoring_overlay_ui.custom_popup_token_rows,
           0,
           sizeof(g_datalab_authoring_overlay_ui.custom_popup_token_rows));
    memset(g_datalab_authoring_overlay_ui.custom_popup_channel_buttons,
           0,
           sizeof(g_datalab_authoring_overlay_ui.custom_popup_channel_buttons));
    memset(g_datalab_authoring_overlay_ui.custom_popup_adjust_buttons,
           0,
           sizeof(g_datalab_authoring_overlay_ui.custom_popup_adjust_buttons));
    memset(g_datalab_authoring_overlay_ui.custom_slot_buttons,
           0,
           sizeof(g_datalab_authoring_overlay_ui.custom_slot_buttons));
    g_datalab_authoring_overlay_ui.custom_popup_assist_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_open_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_create_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_edit_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_rename_button = (SDL_Rect){0, 0, 0, 0};

    pad = datalab_scaled_px(14.0f);
    row_h = datalab_text_line_height(1) + datalab_scaled_px(4.0f);
    button_h = datalab_scaled_px(22.0f);
    button_small_w = datalab_scaled_px(22.0f);
    button_reset_w = datalab_scaled_px(56.0f);
    controls_gap = datalab_scaled_px(6.0f);
    theme_button_gap = datalab_scaled_px(6.0f);

    SDL_SetRenderDrawColor(renderer, palette->clear_r, palette->clear_g, palette->clear_b, 255);
    SDL_RenderClear(renderer);

    panel.x = datalab_scaled_px(20.0f);
    panel.y = datalab_scaled_px(54.0f);
    panel.w = ww - datalab_scaled_px(40.0f);
    panel.h = wh - datalab_scaled_px(84.0f);
    if (panel.w < datalab_scaled_px(240.0f)) {
        panel.w = datalab_scaled_px(240.0f);
    }
    if (panel.h < datalab_scaled_px(200.0f)) {
        panel.h = datalab_scaled_px(200.0f);
    }

    SDL_SetRenderDrawColor(renderer, palette->shell_fill_r, palette->shell_fill_g, palette->shell_fill_b, 246);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
    SDL_RenderDrawRect(renderer, &panel);

    draw_text_5x7(renderer,
                  panel.x + pad,
                  panel.y + pad,
                  "Font/Theme Overlay",
                  1,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);

    section.x = panel.x + pad;
    section.y = panel.y + pad + row_h;
    section.w = panel.w - (pad * 2);
    section.h = datalab_scaled_px(92.0f);
    SDL_SetRenderDrawColor(renderer, palette->button_fill_r, palette->button_fill_g, palette->button_fill_b, 232);
    SDL_RenderFillRect(renderer, &section);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 250);
    SDL_RenderDrawRect(renderer, &section);

    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f),
                  "Font Preset: daw_default",
                  1,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);

    theme_step = datalab_text_zoom_step_clamp(app_state->text_zoom_step);
    zoom_percent = (int)lroundf(datalab_text_zoom_step_multiplier(theme_step) * 100.0f);
    snprintf(size_line, sizeof(size_line), "Text Size step:%d (%d%%)", theme_step, zoom_percent);
    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f) + row_h,
                  size_line,
                  1,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);

    section_inner_x = section.x + datalab_scaled_px(8.0f);
    section_inner_w = section.w - datalab_scaled_px(16.0f);
    controls_y = section.y + section.h - button_h - datalab_scaled_px(8.0f);
    controls_x = section_inner_x;
    g_datalab_authoring_overlay_ui.text_dec_button =
        (SDL_Rect){ controls_x, controls_y, button_small_w, button_h };
    controls_x += button_small_w + controls_gap;
    g_datalab_authoring_overlay_ui.text_inc_button =
        (SDL_Rect){ controls_x, controls_y, button_small_w, button_h };
    controls_x += button_small_w + controls_gap;

    value_chip.w = datalab_overlay_clamp_int(section_inner_w - (button_small_w * 2) - button_reset_w - (controls_gap * 3),
                                             datalab_scaled_px(76.0f),
                                             datalab_scaled_px(140.0f));
    value_chip.h = button_h;
    value_chip.x = controls_x;
    value_chip.y = controls_y;
    controls_x += value_chip.w + controls_gap;

    g_datalab_authoring_overlay_ui.text_reset_button =
        (SDL_Rect){ controls_x, controls_y, button_reset_w, button_h };

    datalab_overlay_draw_button(renderer,
                                &g_datalab_authoring_overlay_ui.text_dec_button,
                                "-",
                                g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_TEXT_DEC,
                                0,
                                palette);
    datalab_overlay_draw_button(renderer,
                                &g_datalab_authoring_overlay_ui.text_inc_button,
                                "+",
                                g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_TEXT_INC,
                                0,
                                palette);

    SDL_SetRenderDrawColor(renderer, palette->clear_r, palette->clear_g, palette->clear_b, 234);
    SDL_RenderFillRect(renderer, &value_chip);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 246);
    SDL_RenderDrawRect(renderer, &value_chip);
    datalab_overlay_draw_centered_text(renderer,
                                       &value_chip,
                                       value_chip.y + datalab_scaled_px(6.0f),
                                       size_line,
                                       palette->text_primary_r,
                                       palette->text_primary_g,
                                       palette->text_primary_b,
                                       255);

    datalab_overlay_draw_button(renderer,
                                &g_datalab_authoring_overlay_ui.text_reset_button,
                                "Reset",
                                g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_TEXT_RESET,
                                0,
                                palette);

    section.y += section.h + datalab_scaled_px(10.0f);
    section.h = datalab_scaled_px(94.0f);
    SDL_SetRenderDrawColor(renderer, palette->button_fill_r, palette->button_fill_g, palette->button_fill_b, 232);
    SDL_RenderFillRect(renderer, &section);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 250);
    SDL_RenderDrawRect(renderer, &section);

    snprintf(line,
             sizeof(line),
             "Theme Preset: %s",
             theme_name ? theme_name : "unknown");
    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f),
                  line,
                  1,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);
    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f) + row_h,
                  "Click a preset to preview live; Apply commits baseline, Cancel reverts draft.",
                  1,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);

    section_inner_x = section.x + datalab_scaled_px(8.0f);
    section_inner_w = section.w - datalab_scaled_px(16.0f);
    theme_button_w = (section_inner_w - (theme_button_gap * (DATALAB_FONT_THEME_THEME_BUTTON_COUNT - 1))) /
                     DATALAB_FONT_THEME_THEME_BUTTON_COUNT;
    if (theme_button_w < datalab_scaled_px(60.0f)) {
        theme_button_w = datalab_scaled_px(60.0f);
    }
    theme_buttons_y = section.y + section.h - button_h - datalab_scaled_px(8.0f);
    controls_x = section_inner_x;
    for (i = 0; i < DATALAB_FONT_THEME_THEME_BUTTON_COUNT; ++i) {
        int is_active = (int)selected_theme == i;
        DatalabAuthoringFontThemeHitId hover_hit =
            (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_THEME_0 + i);
        g_datalab_authoring_overlay_ui.theme_buttons[i] = (SDL_Rect){ controls_x, theme_buttons_y, theme_button_w, button_h };
        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.theme_buttons[i],
                                    k_theme_labels[i],
                                    g_datalab_authoring_overlay_ui.hover_font_hit == hover_hit,
                                    is_active,
                                    palette);
        controls_x += theme_button_w + theme_button_gap;
    }

    custom_section.x = section.x;
    custom_section.y = section.y + section.h + datalab_scaled_px(10.0f);
    custom_section.w = section.w;
    custom_section.h = datalab_scaled_px(132.0f);
    g_datalab_authoring_overlay_ui.custom_open_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_create_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_edit_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_popup_close_button = (SDL_Rect){0, 0, 0, 0};
    if (custom_section.y + custom_section.h < panel.y + panel.h - datalab_scaled_px(30.0f)) {
        section_inner_x = custom_section.x + datalab_scaled_px(8.0f);
        section_inner_w = custom_section.w - datalab_scaled_px(16.0f);
        custom_slot_gap = datalab_scaled_px(6.0f);
        custom_action_gap = datalab_scaled_px(6.0f);
        custom_slot_button_w =
            (section_inner_w - (custom_slot_gap * (DATALAB_CUSTOM_THEME_SLOT_COUNT - 1))) /
            DATALAB_CUSTOM_THEME_SLOT_COUNT;
        if (custom_slot_button_w < datalab_scaled_px(72.0f)) {
            custom_slot_button_w = datalab_scaled_px(72.0f);
        }
        custom_slot_buttons_y = custom_section.y + datalab_scaled_px(8.0f) + (row_h * 2);
        custom_rename_w = datalab_overlay_clamp_int(section_inner_w / 4,
                                                    datalab_scaled_px(56.0f),
                                                    datalab_scaled_px(96.0f));
        custom_action_w =
            (section_inner_w - custom_rename_w - (custom_action_gap * 2)) / 2;
        if (custom_action_w < datalab_scaled_px(64.0f)) {
            custom_action_w = datalab_scaled_px(64.0f);
            custom_rename_w =
                section_inner_w - (custom_action_w * 2) - (custom_action_gap * 2);
            custom_rename_w = datalab_overlay_clamp_int(custom_rename_w,
                                                        datalab_scaled_px(44.0f),
                                                        datalab_scaled_px(96.0f));
            custom_action_w =
                (section_inner_w - custom_rename_w - (custom_action_gap * 2)) / 2;
        }
        custom_action_y = custom_slot_buttons_y + button_h + datalab_scaled_px(8.0f);

        SDL_SetRenderDrawColor(renderer, palette->button_fill_r, palette->button_fill_g, palette->button_fill_b, 224);
        SDL_RenderFillRect(renderer, &custom_section);
        SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 246);
        SDL_RenderDrawRect(renderer, &custom_section);

        draw_text_5x7(renderer,
                      section_inner_x,
                      custom_section.y + datalab_scaled_px(8.0f),
                      "Custom Theme Slots",
                      1,
                      palette->text_primary_r,
                      palette->text_primary_g,
                      palette->text_primary_b,
                      255);
        snprintf(custom_line,
                 sizeof(custom_line),
                 "active:slot %d (%s)",
                 active_custom_slot + 1,
                 app_state->workspace_authoring_custom_theme_slot_names[active_custom_slot]);
        draw_text_5x7(renderer,
                      section_inner_x,
                      custom_section.y + datalab_scaled_px(8.0f) + row_h,
                      custom_line,
                      1,
                      palette->text_secondary_r,
                      palette->text_secondary_g,
                      palette->text_secondary_b,
                      255);

        controls_x = section_inner_x;
        for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
            DatalabAuthoringFontThemeHitId hover_hit =
                (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_0 + i);
            const char *slot_name = app_state->workspace_authoring_custom_theme_slot_names[i];
            if (!slot_name || slot_name[0] == '\0') {
                slot_name = "custom";
            }
            g_datalab_authoring_overlay_ui.custom_slot_buttons[i] = (SDL_Rect){
                controls_x,
                custom_slot_buttons_y,
                custom_slot_button_w,
                button_h
            };
            datalab_overlay_draw_button(renderer,
                                        &g_datalab_authoring_overlay_ui.custom_slot_buttons[i],
                                        slot_name,
                                        g_datalab_authoring_overlay_ui.hover_font_hit == hover_hit,
                                        i == active_custom_slot,
                                        palette);
            controls_x += custom_slot_button_w + custom_slot_gap;
        }

        g_datalab_authoring_overlay_ui.custom_rename_button = (SDL_Rect){
            section_inner_x,
            custom_action_y,
            custom_rename_w,
            button_h
        };
        g_datalab_authoring_overlay_ui.custom_create_button = (SDL_Rect){
            g_datalab_authoring_overlay_ui.custom_rename_button.x +
                g_datalab_authoring_overlay_ui.custom_rename_button.w +
                custom_action_gap,
            custom_action_y,
            custom_action_w,
            button_h
        };
        g_datalab_authoring_overlay_ui.custom_edit_button = (SDL_Rect){
            g_datalab_authoring_overlay_ui.custom_create_button.x +
                g_datalab_authoring_overlay_ui.custom_create_button.w +
                custom_action_gap,
            custom_action_y,
            custom_action_w,
            button_h
        };
        g_datalab_authoring_overlay_ui.custom_open_button = g_datalab_authoring_overlay_ui.custom_edit_button;

        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.custom_rename_button,
                                    "Rename",
                                    g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_CUSTOM_RENAME,
                                    0,
                                    palette);
        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.custom_create_button,
                                    "Create Custom",
                                    g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_CUSTOM_CREATE,
                                    0,
                                    palette);
        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.custom_edit_button,
                                    "Edit Custom",
                                    g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT,
                                    app_state->workspace_authoring_custom_theme_popup_open != 0,
                                    palette);
        draw_text_5x7(renderer,
                      section_inner_x,
                      custom_action_y + button_h + datalab_scaled_px(8.0f),
                      "Create seeds from active preset; Edit opens selected slot editor.",
                      1,
                      palette->text_secondary_r,
                      palette->text_secondary_g,
                      palette->text_secondary_b,
                      255);
        draw_text_5x7(renderer,
                      section_inner_x,
                      custom_action_y + button_h + datalab_scaled_px(20.0f),
                      "Rename cycles stub names. Token lanes: clear/pane/shell/text/button (+ assist).",
                      1,
                      palette->text_secondary_r,
                      palette->text_secondary_g,
                      palette->text_secondary_b,
                      255);
    }

    if (app_state->workspace_authoring_custom_theme_popup_open) {
        int popup_max_h = panel.h - datalab_scaled_px(20.0f);
        int token_value_r = 0;
        int token_value_g = 0;
        int token_value_b = 0;
        int is_row_active = 0;
        int is_row_hover = 0;
        const DatalabWorkspaceCustomTheme *popup_theme =
            datalab_overlay_custom_theme_active_slot_ptr_const(app_state);
        if (!popup_theme) {
            popup_theme = &app_state->workspace_authoring_custom_theme;
        }

        popup.w = datalab_overlay_clamp_int(panel.w - datalab_scaled_px(54.0f),
                                            datalab_scaled_px(320.0f),
                                            datalab_scaled_px(620.0f));
        if (popup_max_h < datalab_scaled_px(220.0f)) {
            popup_max_h = datalab_scaled_px(220.0f);
        }
        popup.h = datalab_overlay_clamp_int(datalab_scaled_px(312.0f),
                                            datalab_scaled_px(220.0f),
                                            popup_max_h);
        popup.x = panel.x + ((panel.w - popup.w) / 2);
        popup.y = panel.y + datalab_scaled_px(42.0f);
        popup_title = (SDL_Rect){ popup.x, popup.y, popup.w, datalab_scaled_px(28.0f) };
        popup_row_h = datalab_scaled_px(16.0f);
        popup_inner_x = popup.x + datalab_scaled_px(12.0f);
        popup_inner_w = popup.w - datalab_scaled_px(24.0f);
        popup_token_label_w = datalab_scaled_px(126.0f);
        popup_row_start_y = popup.y + popup_title.h + datalab_scaled_px(40.0f);
        popup_control_y = popup_row_start_y + (DATALAB_CUSTOM_THEME_TOKEN_COUNT * popup_row_h) + datalab_scaled_px(10.0f);
        popup_channel_button_w = datalab_scaled_px(28.0f);
        popup_adjust_button_w = datalab_scaled_px(34.0f);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, palette->clear_r, palette->clear_g, palette->clear_b, 176);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, palette->shell_fill_r, palette->shell_fill_g, palette->shell_fill_b, 248);
        SDL_RenderFillRect(renderer, &popup);
        SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
        SDL_RenderDrawRect(renderer, &popup);
        SDL_SetRenderDrawColor(renderer, palette->button_fill_r, palette->button_fill_g, palette->button_fill_b, 240);
        SDL_RenderFillRect(renderer, &popup_title);
        SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
        SDL_RenderDrawRect(renderer, &popup_title);

        draw_text_5x7(renderer,
                      popup_title.x + datalab_scaled_px(10.0f),
                      popup_title.y + datalab_scaled_px(8.0f),
                      "Custom Theme Editor",
                      1,
                      palette->text_primary_r,
                      palette->text_primary_g,
                      palette->text_primary_b,
                      255);
        g_datalab_authoring_overlay_ui.custom_popup_close_button = (SDL_Rect){
            popup_title.x + popup_title.w - datalab_scaled_px(28.0f),
            popup_title.y + datalab_scaled_px(4.0f),
            datalab_scaled_px(20.0f),
            datalab_scaled_px(20.0f)
        };
        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.custom_popup_close_button,
                                    "X",
                                    g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_CUSTOM_POPUP_CLOSE,
                                    0,
                                    palette);
        draw_text_5x7(renderer,
                      popup_inner_x,
                      popup.y + popup_title.h + datalab_scaled_px(8.0f),
                      "Rows: click token | Controls: R/G/B then -10/-1/+1/+10 | Assist keeps contrast sane.",
                      1,
                      palette->text_primary_r,
                      palette->text_primary_g,
                      palette->text_primary_b,
                      255);
        snprintf(custom_line,
                 sizeof(custom_line),
                 "slot %d (%s)",
                 active_custom_slot + 1,
                 app_state->workspace_authoring_custom_theme_slot_names[active_custom_slot]);
        draw_text_5x7(renderer,
                      popup_inner_x,
                      popup.y + popup_title.h + datalab_scaled_px(20.0f),
                      custom_line,
                      1,
                      palette->text_secondary_r,
                      palette->text_secondary_g,
                      palette->text_secondary_b,
                      255);

        for (i = 0; i < DATALAB_CUSTOM_THEME_TOKEN_COUNT; ++i) {
            const uint8_t *r_ptr = datalab_overlay_custom_theme_channel_ptr_const(
                popup_theme, i, 0);
            const uint8_t *g_ptr = datalab_overlay_custom_theme_channel_ptr_const(
                popup_theme, i, 1);
            const uint8_t *b_ptr = datalab_overlay_custom_theme_channel_ptr_const(
                popup_theme, i, 2);
            SDL_Rect row_rect = {
                popup_inner_x,
                popup_row_start_y + (i * popup_row_h),
                popup_inner_w,
                popup_row_h - datalab_scaled_px(1.0f)
            };

            token_value_r = r_ptr ? (int)(*r_ptr) : 0;
            token_value_g = g_ptr ? (int)(*g_ptr) : 0;
            token_value_b = b_ptr ? (int)(*b_ptr) : 0;
            g_datalab_authoring_overlay_ui.custom_popup_token_rows[i] = row_rect;
            is_row_active = (i == popup_selected_token);
            is_row_hover = g_datalab_authoring_overlay_ui.hover_font_hit ==
                           (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0 + i);

            if (is_row_active) {
                SDL_SetRenderDrawColor(renderer,
                                       palette->button_active_r,
                                       palette->button_active_g,
                                       palette->button_active_b,
                                       230);
            } else if (is_row_hover) {
                SDL_SetRenderDrawColor(renderer,
                                       palette->button_hover_r,
                                       palette->button_hover_g,
                                       palette->button_hover_b,
                                       220);
            } else {
                SDL_SetRenderDrawColor(renderer,
                                       palette->button_fill_r,
                                       palette->button_fill_g,
                                       palette->button_fill_b,
                                       200);
            }
            SDL_RenderFillRect(renderer, &row_rect);
            SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 238);
            SDL_RenderDrawRect(renderer, &row_rect);

            draw_text_5x7(renderer,
                          row_rect.x + datalab_scaled_px(6.0f),
                          row_rect.y + datalab_scaled_px(4.0f),
                          k_custom_theme_tokens[i],
                          1,
                          palette->text_primary_r,
                          palette->text_primary_g,
                          palette->text_primary_b,
                          255);
            snprintf(popup_row_line,
                     sizeof(popup_row_line),
                     "R:%03d  G:%03d  B:%03d",
                     token_value_r,
                     token_value_g,
                     token_value_b);
            draw_text_5x7(renderer,
                          row_rect.x + popup_token_label_w,
                          row_rect.y + datalab_scaled_px(4.0f),
                          popup_row_line,
                          1,
                          palette->text_secondary_r,
                          palette->text_secondary_g,
                          palette->text_secondary_b,
                          255);
        }

        controls_x = popup_inner_x;
        for (channel = 0; channel < DATALAB_CUSTOM_THEME_CHANNEL_COUNT; ++channel) {
            const char *label = (channel == 0) ? "R" : ((channel == 1) ? "G" : "B");
            DatalabAuthoringFontThemeHitId hover_id =
                (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_R + channel);
            int is_channel_active = popup_selected_channel == channel;
            SDL_Rect channel_rect = {
                controls_x,
                popup_control_y,
                popup_channel_button_w,
                button_h
            };
            g_datalab_authoring_overlay_ui.custom_popup_channel_buttons[channel] = channel_rect;
            datalab_overlay_draw_button(renderer,
                                        &channel_rect,
                                        label,
                                        g_datalab_authoring_overlay_ui.hover_font_hit == hover_id,
                                        is_channel_active,
                                        palette);
            controls_x += popup_channel_button_w + datalab_scaled_px(4.0f);
        }

        controls_x += datalab_scaled_px(8.0f);
        for (i = 0; i < 4; ++i) {
            static const char *k_adjust_labels[4] = { "-10", "-1", "+1", "+10" };
            DatalabAuthoringFontThemeHitId hover_id =
                (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_10 + i);
            SDL_Rect adjust_rect = {
                controls_x,
                popup_control_y,
                popup_adjust_button_w,
                button_h
            };
            g_datalab_authoring_overlay_ui.custom_popup_adjust_buttons[i] = adjust_rect;
            datalab_overlay_draw_button(renderer,
                                        &adjust_rect,
                                        k_adjust_labels[i],
                                        g_datalab_authoring_overlay_ui.hover_font_hit == hover_id,
                                        0,
                                        palette);
            controls_x += popup_adjust_button_w + datalab_scaled_px(4.0f);
        }

        controls_x += datalab_scaled_px(8.0f);
        g_datalab_authoring_overlay_ui.custom_popup_assist_button = (SDL_Rect){
            controls_x,
            popup_control_y,
            datalab_scaled_px(84.0f),
            button_h
        };
        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.custom_popup_assist_button,
                                    "Assist",
                                    g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_CUSTOM_ASSIST,
                                    0,
                                    palette);

        {
            const uint8_t *r_ptr = datalab_overlay_custom_theme_channel_ptr_const(
                popup_theme, popup_selected_token, 0);
            const uint8_t *g_ptr = datalab_overlay_custom_theme_channel_ptr_const(
                popup_theme, popup_selected_token, 1);
            const uint8_t *b_ptr = datalab_overlay_custom_theme_channel_ptr_const(
                popup_theme, popup_selected_token, 2);
            SDL_Rect swatch_rect = {
                popup.x + popup.w - datalab_scaled_px(74.0f),
                popup_control_y,
                datalab_scaled_px(56.0f),
                button_h
            };

            token_value_r = r_ptr ? (int)(*r_ptr) : 0;
            token_value_g = g_ptr ? (int)(*g_ptr) : 0;
            token_value_b = b_ptr ? (int)(*b_ptr) : 0;
            SDL_SetRenderDrawColor(renderer, (uint8_t)token_value_r, (uint8_t)token_value_g, (uint8_t)token_value_b, 255);
            SDL_RenderFillRect(renderer, &swatch_rect);
            SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
            SDL_RenderDrawRect(renderer, &swatch_rect);
            snprintf(popup_row_line,
                     sizeof(popup_row_line),
                     "active:%s  [%03d,%03d,%03d]",
                     k_custom_theme_tokens[popup_selected_token],
                     token_value_r,
                     token_value_g,
                     token_value_b);
            draw_text_5x7(renderer,
                          popup_inner_x,
                          popup_control_y + button_h + datalab_scaled_px(8.0f),
                          popup_row_line,
                          1,
                          palette->text_primary_r,
                          palette->text_primary_g,
                          palette->text_primary_b,
                          255);
            draw_text_5x7(renderer,
                          popup_inner_x,
                          popup_control_y + button_h + datalab_scaled_px(20.0f),
                          k_custom_theme_token_desc[popup_selected_token],
                          1,
                          palette->text_secondary_r,
                          palette->text_secondary_g,
                          palette->text_secondary_b,
                          255);
        }

        draw_text_5x7(renderer,
                      popup_inner_x,
                      popup.y + popup.h - datalab_scaled_px(20.0f),
                      "Keyboard: Up/Down token | Left/Right channel | +/- adjust | A assist | Esc close.",
                      1,
                      palette->text_secondary_r,
                      palette->text_secondary_g,
                      palette->text_secondary_b,
                      255);
    }

    snprintf(line,
             sizeof(line),
             "pending:%u apply:%u cancel:%u overlay_cycles:%u",
             (unsigned int)app_state->workspace_authoring_pending_stub,
             (unsigned int)app_state->workspace_authoring_apply_count,
             (unsigned int)app_state->workspace_authoring_cancel_count,
             (unsigned int)app_state->workspace_authoring_overlay_cycle_count);
    draw_text_5x7(renderer,
                  panel.x + pad,
                  panel.y + panel.h - pad - datalab_text_line_height(1),
                  line,
                  1,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);

    g_datalab_authoring_overlay_ui.font_controls_valid = 1u;
}
