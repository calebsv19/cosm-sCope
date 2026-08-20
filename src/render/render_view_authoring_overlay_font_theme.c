#include "render/render_view_authoring_overlay_shared.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "kit_render.h"
#include "kit_ui_sdl.h"

/* Text sizing is owned by the shared layout's KitRenderContext.  The SDL text
 * adapter receives the same scale, so it must not apply a second host-only
 * multiplier. */
enum { DATALAB_AUTHORING_TEXT_SCALE = 1 };

static int datalab_authoring_text_measure(void *user,
                                          const char *text,
                                          int scale,
                                          int *out_width,
                                          int *out_height) {
    (void)user;
    return datalab_measure_text(scale, text, out_width, out_height);
}

static int datalab_authoring_text_line_height(void *user, int scale) {
    (void)user;
    return datalab_text_line_height(scale);
}

static void datalab_authoring_draw_text_clipped(void *user,
                                                SDL_Renderer *renderer,
                                                const SDL_Rect *clip_rect,
                                                int x,
                                                int y,
                                                const char *text,
                                                int scale,
                                                KitRenderColor color) {
    (void)user;
    draw_text_5x7_clipped(renderer,
                          clip_rect,
                          x,
                          y,
                          text,
                          scale,
                          color.r,
                          color.g,
                          color.b,
                          color.a);
}

static CoreThemePresetId datalab_authoring_core_theme_preset(
    DatalabWorkspaceAuthoringThemePreset preset) {
    switch (preset) {
        case DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT:
            return CORE_THEME_PRESET_DAW_DEFAULT;
        case DATALAB_WORKSPACE_AUTHORING_THEME_STANDARD_GREY:
            return CORE_THEME_PRESET_IDE_GRAY;
        case DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT:
            return CORE_THEME_PRESET_LIGHT_DEFAULT;
        case DATALAB_WORKSPACE_AUTHORING_THEME_GREYSCALE:
            return CORE_THEME_PRESET_GREYSCALE;
        case DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM:
        case DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST:
        default:
            return CORE_THEME_PRESET_DARK_DEFAULT;
    }
}

static int datalab_authoring_build_font_theme_layout(const DatalabAppState *app_state,
                                                     int viewport_width,
                                                     int viewport_height,
                                                     KitWorkspaceAuthoringFontThemeLayout *out_layout) {
    KitRenderContext kit_ctx;
    CoreResult result;
    CoreFontPresetId font_preset = CORE_FONT_PRESET_IDE;

    if (!app_state || !out_layout) {
        return 0;
    }
    memset(&kit_ctx, 0, sizeof(kit_ctx));
    result = kit_render_context_init(&kit_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     datalab_authoring_core_theme_preset(
                                         datalab_overlay_selected_theme(app_state)),
                                     font_preset);
    if (result.code == CORE_OK) {
        (void)kit_render_set_text_zoom_step(&kit_ctx,
                                            datalab_text_zoom_step_clamp(app_state->text_zoom_step));
    }
    if (!kit_workspace_authoring_ui_font_theme_build_layout(result.code == CORE_OK ? &kit_ctx : NULL,
                                                            viewport_width,
                                                            viewport_height,
                                                            out_layout)) {
        if (result.code == CORE_OK) {
            kit_render_context_shutdown(&kit_ctx);
        }
        return 0;
    }
    if (result.code == CORE_OK) {
        kit_render_context_shutdown(&kit_ctx);
    }
    return 1;
}

static SDL_Rect datalab_overlay_rect_from_kit(KitRenderRect rect) {
    SDL_Rect out = {
        (int)lroundf(rect.x),
        (int)lroundf(rect.y),
        (int)lroundf(rect.width),
        (int)lroundf(rect.height)
    };
    if (out.w < 0) {
        out.w = 0;
    }
    if (out.h < 0) {
        out.h = 0;
    }
    return out;
}

static KitWorkspaceAuthoringFontThemeButtonId datalab_overlay_shared_font_button_id_at(int index) {
    switch (index) {
        case 0:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT;
        case 1:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_IDE;
        case 2:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_CUSTOM_STUB;
        default:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
    }
}

static KitWorkspaceAuthoringFontThemeButtonId datalab_overlay_shared_theme_button_id_at(int index) {
    switch (index) {
        case 0:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT;
        case 1:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_STANDARD_GREY;
        case 2:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_MIDNIGHT_CONTRAST;
        case 3:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_SOFT_LIGHT;
        case 4:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_GREYSCALE;
        default:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
    }
}

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
    (void)datalab_measure_text(DATALAB_AUTHORING_TEXT_SCALE, text, &text_w, &text_h);
    x = rect->x + ((rect->w - text_w) / 2);
    if (x < rect->x + datalab_scaled_px(6.0f)) {
        x = rect->x + datalab_scaled_px(6.0f);
    }
    draw_text_5x7(renderer, x, y, text, DATALAB_AUTHORING_TEXT_SCALE, r, g, b, a);
}

void datalab_overlay_draw_button(SDL_Renderer *renderer,
                                        const SDL_Rect *rect,
                                        const char *label,
                                        int hover,
                                        int active,
                                        const DatalabAuthoringThemePalette *palette) {
    KitUiHudStyle style;
    KitUiButtonState state;
    KitUiSdlTextApi text_api;

    if (!renderer || !rect || !label || !palette) {
        return;
    }

    datalab_overlay_hud_style_from_palette(palette, &style);
    style.button_corner_radius = (float)datalab_scaled_px(style.button_corner_radius);
    kit_ui_button_state_init(&state);
    state.hovered = hover != 0;
    state.selected = active != 0;
    text_api = (KitUiSdlTextApi){
        0,
        DATALAB_AUTHORING_TEXT_SCALE,
        datalab_scaled_px(8.0f),
        datalab_authoring_text_measure,
        datalab_authoring_text_line_height,
        datalab_authoring_draw_text_clipped
    };
    kit_ui_sdl_draw_button(renderer, rect, label, &state, &style, &text_api);
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
    SDL_Rect button_rect = {0};
    SDL_Rect popup = {0};
    SDL_Rect popup_title = {0};
    int pad = 0;
    int row_h = 0;
    int button_h = 0;
    int controls_x = 0;
    int section_inner_x = 0;
    int section_inner_w = 0;
    int i = 0;
    int theme_step = 0;
    int zoom_percent = 100;
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
    int custom_action_y = 0;
    int custom_rename_w = 0;
    int active_custom_slot = 0;
    char line[192];
    char size_line[80];
    char custom_line[160];
    char popup_row_line[96];
    KitWorkspaceAuthoringFontThemeLayout shared_layout = {0};
    DatalabWorkspaceAuthoringThemePreset selected_theme = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    const char *theme_name = NULL;
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
    g_datalab_authoring_overlay_ui.font_theme_shared_layout_valid =
        (uint8_t)datalab_authoring_build_font_theme_layout(app_state, ww, wh, &shared_layout);
    if (!g_datalab_authoring_overlay_ui.font_theme_shared_layout_valid) {
        g_datalab_authoring_overlay_ui.font_controls_valid = 0u;
        return;
    }
    g_datalab_authoring_overlay_ui.font_theme_shared_layout = shared_layout;

    pad = datalab_scaled_px(14.0f);
    row_h = datalab_text_line_height(DATALAB_AUTHORING_TEXT_SCALE) + datalab_scaled_px(5.0f);
    button_h = datalab_scaled_px(22.0f);

    SDL_SetRenderDrawColor(renderer, palette->clear_r, palette->clear_g, palette->clear_b, 255);
    SDL_RenderClear(renderer);

    panel = datalab_overlay_rect_from_kit(shared_layout.panel);

    {
        KitUiHudStyle style;
        datalab_overlay_hud_style_from_palette(palette, &style);
        style.panel_corner_radius = (float)datalab_scaled_px(style.panel_corner_radius);
        kit_ui_sdl_fill_rounded_rect(renderer, &panel, (int)style.panel_corner_radius, style.panel_fill);
    }

    draw_text_5x7(renderer,
                  panel.x + pad,
                  panel.y + pad,
                  "Workspace appearance",
                  DATALAB_AUTHORING_TEXT_SCALE,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);

    section = datalab_overlay_rect_from_kit(shared_layout.font_preset_section);
    {
        KitUiHudStyle style;
        datalab_overlay_hud_style_from_palette(palette, &style);
        kit_ui_sdl_fill_rounded_rect(renderer,
                                     &section,
                                     datalab_scaled_px(4.0f),
                                     style.readout_fill);
    }

    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f),
                  "Font preset: IDE",
                  DATALAB_AUTHORING_TEXT_SCALE,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);

    for (i = 0; i < (int)shared_layout.font_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id = datalab_overlay_shared_font_button_id_at(i);
        const char *label = kit_workspace_authoring_ui_font_theme_button_label(button_id);
        button_rect = datalab_overlay_rect_from_kit(shared_layout.font_preset_buttons[i]);
        datalab_overlay_draw_button(renderer,
                                    &button_rect,
                                    label ? label : "font",
                                    g_datalab_authoring_overlay_ui.hover_font_hit ==
                                        (DatalabAuthoringFontThemeHitId)button_id,
                                    button_id == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_IDE,
                                    palette);
    }

    section = datalab_overlay_rect_from_kit(shared_layout.text_size_section);
    {
        KitUiHudStyle style;
        datalab_overlay_hud_style_from_palette(palette, &style);
        kit_ui_sdl_fill_rounded_rect(renderer,
                                     &section,
                                     datalab_scaled_px(4.0f),
                                     style.readout_fill);
    }
    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f),
                  "Interface scale",
                  DATALAB_AUTHORING_TEXT_SCALE,
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
                  DATALAB_AUTHORING_TEXT_SCALE,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);

    g_datalab_authoring_overlay_ui.text_dec_button =
        datalab_overlay_rect_from_kit(shared_layout.text_size_dec_button);
    g_datalab_authoring_overlay_ui.text_inc_button =
        datalab_overlay_rect_from_kit(shared_layout.text_size_inc_button);
    value_chip = datalab_overlay_rect_from_kit(shared_layout.text_size_value_chip);
    g_datalab_authoring_overlay_ui.text_reset_button =
        datalab_overlay_rect_from_kit(shared_layout.text_size_reset_button);

    datalab_overlay_draw_button(renderer,
                                &g_datalab_authoring_overlay_ui.text_dec_button,
                                kit_workspace_authoring_ui_font_theme_button_label(
                                    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC),
                                g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_TEXT_DEC,
                                0,
                                palette);
    datalab_overlay_draw_button(renderer,
                                &g_datalab_authoring_overlay_ui.text_inc_button,
                                kit_workspace_authoring_ui_font_theme_button_label(
                                    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC),
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
                                kit_workspace_authoring_ui_font_theme_button_label(
                                    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET),
                                g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_TEXT_RESET,
                                0,
                                palette);

    section = datalab_overlay_rect_from_kit(shared_layout.theme_preset_section);
    {
        KitUiHudStyle style;
        datalab_overlay_hud_style_from_palette(palette, &style);
        kit_ui_sdl_fill_rounded_rect(renderer,
                                     &section,
                                     datalab_scaled_px(4.0f),
                                     style.readout_fill);
    }

    snprintf(line,
             sizeof(line),
             "Theme Preset: %s",
             theme_name ? theme_name : "unknown");
    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f),
                  line,
                  DATALAB_AUTHORING_TEXT_SCALE,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);
    draw_text_5x7(renderer,
                  section.x + datalab_scaled_px(8.0f),
                  section.y + datalab_scaled_px(8.0f) + row_h,
                  "Choose a preset to preview. Apply keeps it; Cancel restores the prior appearance.",
                  DATALAB_AUTHORING_TEXT_SCALE,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);

    for (i = 0; i < (int)shared_layout.theme_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id = datalab_overlay_shared_theme_button_id_at(i);
        int is_active = (int)selected_theme == i;
        DatalabAuthoringFontThemeHitId hover_hit =
            (DatalabAuthoringFontThemeHitId)(DATALAB_AUTHORING_FONT_HIT_THEME_0 + i);
        g_datalab_authoring_overlay_ui.theme_buttons[i] =
            datalab_overlay_rect_from_kit(shared_layout.theme_preset_buttons[i]);
        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.theme_buttons[i],
                                    kit_workspace_authoring_ui_font_theme_button_label(button_id),
                                    g_datalab_authoring_overlay_ui.hover_font_hit == hover_hit,
                                    is_active,
                                    palette);
    }

    custom_section = datalab_overlay_rect_from_kit(shared_layout.custom_theme_section);
    g_datalab_authoring_overlay_ui.custom_open_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_create_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_edit_button = (SDL_Rect){0, 0, 0, 0};
    g_datalab_authoring_overlay_ui.custom_popup_close_button = (SDL_Rect){0, 0, 0, 0};
    if (custom_section.y + custom_section.h <= panel.y + panel.h - datalab_scaled_px(8.0f)) {
        section_inner_x = custom_section.x + datalab_scaled_px(8.0f);
        section_inner_w = custom_section.w - datalab_scaled_px(16.0f);
        custom_slot_gap = datalab_scaled_px(6.0f);
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
        custom_action_y = custom_slot_buttons_y + button_h + datalab_scaled_px(8.0f);

        {
            KitUiHudStyle style;
            datalab_overlay_hud_style_from_palette(palette, &style);
            kit_ui_sdl_fill_rounded_rect(renderer,
                                         &custom_section,
                                         datalab_scaled_px(4.0f),
                                         style.readout_fill);
        }

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

        g_datalab_authoring_overlay_ui.custom_create_button =
            datalab_overlay_rect_from_kit(shared_layout.custom_theme_buttons[0]);
        g_datalab_authoring_overlay_ui.custom_edit_button =
            datalab_overlay_rect_from_kit(shared_layout.custom_theme_buttons[1]);
        g_datalab_authoring_overlay_ui.custom_rename_button = (SDL_Rect){
            section_inner_x,
            custom_action_y,
            custom_rename_w,
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
                                        "Save current preset",
                                    g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_CUSTOM_CREATE,
                                    0,
                                    palette);
        datalab_overlay_draw_button(renderer,
                                    &g_datalab_authoring_overlay_ui.custom_edit_button,
                                        "Edit colors",
                                    g_datalab_authoring_overlay_ui.hover_font_hit == DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT,
                                    app_state->workspace_authoring_custom_theme_popup_open != 0,
                                    palette);
        draw_text_5x7(renderer,
                      section_inner_x,
                      custom_action_y + button_h + datalab_scaled_px(8.0f),
                      "Save copies the current preset into this local slot. Edit adjusts its colors.",
                      DATALAB_AUTHORING_TEXT_SCALE,
                      palette->text_secondary_r,
                      palette->text_secondary_g,
                      palette->text_secondary_b,
                      255);
        draw_text_5x7(renderer,
                      section_inner_x,
                      custom_action_y + button_h + datalab_scaled_px(20.0f),
                      "Select a slot, then use Edit colors when you need a custom palette.",
                      DATALAB_AUTHORING_TEXT_SCALE,
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
             "Preview changes are local until Apply. Esc or Cancel returns to DataLab unchanged.");
    draw_text_5x7(renderer,
                  panel.x + pad,
                  panel.y + panel.h - pad - datalab_text_line_height(DATALAB_AUTHORING_TEXT_SCALE),
                  line,
                  DATALAB_AUTHORING_TEXT_SCALE,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);

    g_datalab_authoring_overlay_ui.font_controls_valid = 1u;
}
