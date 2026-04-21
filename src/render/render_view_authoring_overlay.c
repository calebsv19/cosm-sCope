#include "render/render_view_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "kit_workspace_authoring_ui.h"

enum {
    DATALAB_AUTHORING_LAYOUT_MODE_RUNTIME = 0,
    DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING = 1,
    DATALAB_FONT_THEME_THEME_BUTTON_COUNT = 6,
    DATALAB_CUSTOM_THEME_TOKEN_COUNT = 9,
    DATALAB_CUSTOM_THEME_CHANNEL_COUNT = 3
};

typedef enum DatalabAuthoringFontThemeHitId {
    DATALAB_AUTHORING_FONT_HIT_NONE = 0,
    DATALAB_AUTHORING_FONT_HIT_TEXT_DEC = 1,
    DATALAB_AUTHORING_FONT_HIT_TEXT_INC = 2,
    DATALAB_AUTHORING_FONT_HIT_TEXT_RESET = 3,
    DATALAB_AUTHORING_FONT_HIT_THEME_0 = 10,
    DATALAB_AUTHORING_FONT_HIT_THEME_1 = 11,
    DATALAB_AUTHORING_FONT_HIT_THEME_2 = 12,
    DATALAB_AUTHORING_FONT_HIT_THEME_3 = 13,
    DATALAB_AUTHORING_FONT_HIT_THEME_4 = 14,
    DATALAB_AUTHORING_FONT_HIT_THEME_5 = 15,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_OPEN = 20,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_POPUP_CLOSE = 21,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_CREATE = 22,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT = 23,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_0 = 24,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_1 = 25,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_2 = 26,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_RENAME = 27,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0 = 100,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_8 = 108,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_R = 120,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_G = 121,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_B = 122,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_10 = 130,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_1 = 131,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_INC_1 = 132,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_INC_10 = 133,
    DATALAB_AUTHORING_FONT_HIT_CUSTOM_ASSIST = 134
} DatalabAuthoringFontThemeHitId;

typedef struct DatalabAuthoringThemePalette {
    uint8_t clear_r;
    uint8_t clear_g;
    uint8_t clear_b;
    uint8_t pane_fill_r;
    uint8_t pane_fill_g;
    uint8_t pane_fill_b;
    uint8_t shell_fill_r;
    uint8_t shell_fill_g;
    uint8_t shell_fill_b;
    uint8_t shell_border_r;
    uint8_t shell_border_g;
    uint8_t shell_border_b;
    uint8_t text_primary_r;
    uint8_t text_primary_g;
    uint8_t text_primary_b;
    uint8_t text_secondary_r;
    uint8_t text_secondary_g;
    uint8_t text_secondary_b;
    uint8_t button_fill_r;
    uint8_t button_fill_g;
    uint8_t button_fill_b;
    uint8_t button_hover_r;
    uint8_t button_hover_g;
    uint8_t button_hover_b;
    uint8_t button_active_r;
    uint8_t button_active_g;
    uint8_t button_active_b;
} DatalabAuthoringThemePalette;

typedef struct DatalabAuthoringOverlayUiState {
    int viewport_w;
    int viewport_h;
    int pane_overlay_active;
    KitWorkspaceAuthoringOverlayButton top_buttons[8];
    uint32_t top_button_count;
    KitWorkspaceAuthoringOverlayButtonId hover_top_button;
    DatalabAuthoringFontThemeHitId hover_font_hit;
    uint8_t font_controls_valid;
    SDL_Rect text_dec_button;
    SDL_Rect text_inc_button;
    SDL_Rect text_reset_button;
    SDL_Rect theme_buttons[DATALAB_FONT_THEME_THEME_BUTTON_COUNT];
    SDL_Rect custom_open_button;
    SDL_Rect custom_create_button;
    SDL_Rect custom_edit_button;
    SDL_Rect custom_slot_buttons[DATALAB_CUSTOM_THEME_SLOT_COUNT];
    SDL_Rect custom_rename_button;
    SDL_Rect custom_popup_close_button;
    SDL_Rect custom_popup_token_rows[DATALAB_CUSTOM_THEME_TOKEN_COUNT];
    SDL_Rect custom_popup_channel_buttons[DATALAB_CUSTOM_THEME_CHANNEL_COUNT];
    SDL_Rect custom_popup_adjust_buttons[4];
    SDL_Rect custom_popup_assist_button;
} DatalabAuthoringOverlayUiState;

typedef struct DatalabAuthoringOverlayHost {
    SDL_Renderer *renderer;
    const DatalabFrame *frame;
    const DatalabAppState *app_state;
} DatalabAuthoringOverlayHost;

static DatalabAuthoringOverlayUiState g_datalab_authoring_overlay_ui = {0};

static int datalab_overlay_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int datalab_overlay_point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) {
        return 0;
    }
    return x >= rect->x && x < (rect->x + rect->w) && y >= rect->y && y < (rect->y + rect->h);
}

static SDL_Rect datalab_overlay_rect_from_core(CorePaneRect rect) {
    SDL_Rect out = { 0, 0, 0, 0 };
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    return out;
}

static const char *datalab_overlay_profile_name(const DatalabFrame *frame) {
    if (!frame) {
        return "unknown";
    }
    switch (frame->profile) {
        case DATALAB_PROFILE_PHYSICS:
            return "physics";
        case DATALAB_PROFILE_DAW:
            return "daw";
        case DATALAB_PROFILE_TRACE:
            return "trace";
        default:
            return "unknown";
    }
}

static DatalabWorkspaceAuthoringThemePreset datalab_overlay_theme_preset_clamp(int value) {
    if (value < (int)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT) {
        return DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT;
    }
    if (value > (int)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        return DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    }
    return (DatalabWorkspaceAuthoringThemePreset)value;
}

static const char *datalab_overlay_theme_name(DatalabWorkspaceAuthoringThemePreset preset) {
    switch (preset) {
        case DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT:
            return "daw_default";
        case DATALAB_WORKSPACE_AUTHORING_THEME_STANDARD_GREY:
            return "standard_grey";
        case DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST:
            return "midnight_contrast";
        case DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT:
            return "soft_light";
        case DATALAB_WORKSPACE_AUTHORING_THEME_GREYSCALE:
            return "greyscale";
        case DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM:
            return "custom";
        default:
            return "unknown";
    }
}

static void datalab_overlay_theme_palette(DatalabWorkspaceAuthoringThemePreset preset,
                                          const DatalabWorkspaceCustomTheme *custom_theme,
                                          DatalabAuthoringThemePalette *out_palette) {
    DatalabAuthoringThemePalette palette = {
        12, 14, 20,
        54, 36, 74,
        24, 28, 38,
        112, 124, 146,
        226, 234, 246,
        178, 194, 220,
        34, 40, 58,
        48, 58, 84,
        116, 136, 184
    };

    switch (preset) {
        case DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT:
            palette = (DatalabAuthoringThemePalette){
                10, 12, 22,
                28, 44, 82,
                18, 28, 52,
                102, 132, 188,
                232, 240, 252,
                180, 198, 226,
                26, 44, 76,
                40, 62, 104,
                78, 128, 206
            };
            break;
        case DATALAB_WORKSPACE_AUTHORING_THEME_STANDARD_GREY:
            palette = (DatalabAuthoringThemePalette){
                20, 22, 26,
                58, 60, 68,
                34, 36, 42,
                124, 128, 138,
                236, 238, 244,
                188, 194, 206,
                44, 48, 58,
                62, 68, 82,
                150, 156, 170
            };
            break;
        case DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT:
            palette = (DatalabAuthoringThemePalette){
                232, 236, 244,
                208, 218, 236,
                240, 244, 252,
                148, 162, 186,
                40, 48, 66,
                76, 90, 116,
                220, 228, 242,
                206, 220, 240,
                172, 194, 232
            };
            break;
        case DATALAB_WORKSPACE_AUTHORING_THEME_GREYSCALE:
            palette = (DatalabAuthoringThemePalette){
                18, 18, 18,
                52, 52, 52,
                28, 28, 28,
                122, 122, 122,
                232, 232, 232,
                184, 184, 184,
                42, 42, 42,
                62, 62, 62,
                142, 142, 142
            };
            break;
        case DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM:
            if (custom_theme) {
                palette = (DatalabAuthoringThemePalette){
                    custom_theme->clear_r, custom_theme->clear_g, custom_theme->clear_b,
                    custom_theme->pane_fill_r, custom_theme->pane_fill_g, custom_theme->pane_fill_b,
                    custom_theme->shell_fill_r, custom_theme->shell_fill_g, custom_theme->shell_fill_b,
                    custom_theme->shell_border_r, custom_theme->shell_border_g, custom_theme->shell_border_b,
                    custom_theme->text_primary_r, custom_theme->text_primary_g, custom_theme->text_primary_b,
                    custom_theme->text_secondary_r, custom_theme->text_secondary_g, custom_theme->text_secondary_b,
                    custom_theme->button_fill_r, custom_theme->button_fill_g, custom_theme->button_fill_b,
                    custom_theme->button_hover_r, custom_theme->button_hover_g, custom_theme->button_hover_b,
                    custom_theme->button_active_r, custom_theme->button_active_g, custom_theme->button_active_b
                };
            }
            break;
        case DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST:
        default:
            break;
    }

    if (out_palette) {
        *out_palette = palette;
    }
}

static DatalabWorkspaceCustomTheme datalab_overlay_custom_theme_from_palette(
    const DatalabAuthoringThemePalette *palette) {
    DatalabWorkspaceCustomTheme theme = {0};

    if (!palette) {
        return theme;
    }
    theme.clear_r = palette->clear_r;
    theme.clear_g = palette->clear_g;
    theme.clear_b = palette->clear_b;
    theme.pane_fill_r = palette->pane_fill_r;
    theme.pane_fill_g = palette->pane_fill_g;
    theme.pane_fill_b = palette->pane_fill_b;
    theme.shell_fill_r = palette->shell_fill_r;
    theme.shell_fill_g = palette->shell_fill_g;
    theme.shell_fill_b = palette->shell_fill_b;
    theme.shell_border_r = palette->shell_border_r;
    theme.shell_border_g = palette->shell_border_g;
    theme.shell_border_b = palette->shell_border_b;
    theme.text_primary_r = palette->text_primary_r;
    theme.text_primary_g = palette->text_primary_g;
    theme.text_primary_b = palette->text_primary_b;
    theme.text_secondary_r = palette->text_secondary_r;
    theme.text_secondary_g = palette->text_secondary_g;
    theme.text_secondary_b = palette->text_secondary_b;
    theme.button_fill_r = palette->button_fill_r;
    theme.button_fill_g = palette->button_fill_g;
    theme.button_fill_b = palette->button_fill_b;
    theme.button_hover_r = palette->button_hover_r;
    theme.button_hover_g = palette->button_hover_g;
    theme.button_hover_b = palette->button_hover_b;
    theme.button_active_r = palette->button_active_r;
    theme.button_active_g = palette->button_active_g;
    theme.button_active_b = palette->button_active_b;
    return theme;
}

static DatalabWorkspaceAuthoringThemePreset datalab_overlay_selected_theme(const DatalabAppState *app_state) {
    if (!app_state) {
        return DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    }
    return datalab_overlay_theme_preset_clamp((int)app_state->workspace_authoring_theme_preset_id);
}

static int datalab_overlay_custom_theme_token_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_TOKEN_COUNT) {
        return DATALAB_CUSTOM_THEME_TOKEN_COUNT - 1;
    }
    return value;
}

static int datalab_overlay_custom_theme_channel_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_CHANNEL_COUNT) {
        return DATALAB_CUSTOM_THEME_CHANNEL_COUNT - 1;
    }
    return value;
}

static uint8_t *datalab_overlay_custom_theme_channel_ptr(DatalabWorkspaceCustomTheme *theme,
                                                         int token_index,
                                                         int channel_index) {
    uint8_t *token = NULL;
    if (!theme) {
        return NULL;
    }
    switch (datalab_overlay_custom_theme_token_clamp(token_index)) {
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
    return token + datalab_overlay_custom_theme_channel_clamp(channel_index);
}

static const uint8_t *datalab_overlay_custom_theme_channel_ptr_const(const DatalabWorkspaceCustomTheme *theme,
                                                                     int token_index,
                                                                     int channel_index) {
    return datalab_overlay_custom_theme_channel_ptr((DatalabWorkspaceCustomTheme *)theme,
                                                    token_index,
                                                    channel_index);
}

static int datalab_overlay_custom_theme_slot_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return DATALAB_CUSTOM_THEME_SLOT_COUNT - 1;
    }
    return value;
}

static DatalabWorkspaceCustomTheme *datalab_overlay_custom_theme_active_slot_ptr(DatalabAppState *app_state) {
    int slot_index = 0;
    if (!app_state) {
        return NULL;
    }
    slot_index = datalab_overlay_custom_theme_slot_clamp((int)app_state->workspace_authoring_custom_theme_active_slot);
    app_state->workspace_authoring_custom_theme_active_slot = (uint8_t)slot_index;
    return &app_state->workspace_authoring_custom_theme_slots[slot_index];
}

static const DatalabWorkspaceCustomTheme *datalab_overlay_custom_theme_active_slot_ptr_const(
    const DatalabAppState *app_state) {
    int slot_index = 0;
    if (!app_state) {
        return NULL;
    }
    slot_index = datalab_overlay_custom_theme_slot_clamp((int)app_state->workspace_authoring_custom_theme_active_slot);
    return &app_state->workspace_authoring_custom_theme_slots[slot_index];
}

static void datalab_overlay_sync_custom_theme_from_active_slot(DatalabAppState *app_state) {
    const DatalabWorkspaceCustomTheme *slot_theme =
        datalab_overlay_custom_theme_active_slot_ptr_const(app_state);
    if (!slot_theme) {
        return;
    }
    app_state->workspace_authoring_custom_theme = *slot_theme;
}

static float datalab_overlay_color_lerp(float a, float b, float t) {
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    return a + ((b - a) * t);
}

static uint8_t datalab_overlay_color_channel_from_unit(float value) {
    int i = 0;
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 1.0f) {
        value = 1.0f;
    }
    i = (int)lroundf(value * 255.0f);
    if (i < 0) {
        i = 0;
    } else if (i > 255) {
        i = 255;
    }
    return (uint8_t)i;
}

static float datalab_overlay_channel_to_linear(uint8_t channel) {
    float c = ((float)channel) / 255.0f;
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float datalab_overlay_relative_luminance(uint8_t r, uint8_t g, uint8_t b) {
    float rl = datalab_overlay_channel_to_linear(r);
    float gl = datalab_overlay_channel_to_linear(g);
    float bl = datalab_overlay_channel_to_linear(b);
    return (0.2126f * rl) + (0.7152f * gl) + (0.0722f * bl);
}

static float datalab_overlay_contrast_ratio_rgb(uint8_t ar, uint8_t ag, uint8_t ab,
                                                uint8_t br, uint8_t bg, uint8_t bb) {
    float la = datalab_overlay_relative_luminance(ar, ag, ab);
    float lb = datalab_overlay_relative_luminance(br, bg, bb);
    float lighter = la > lb ? la : lb;
    float darker = la > lb ? lb : la;
    return (lighter + 0.05f) / (darker + 0.05f);
}

static void datalab_overlay_set_color_mix(uint8_t base_r,
                                          uint8_t base_g,
                                          uint8_t base_b,
                                          float white_mix,
                                          float black_mix,
                                          uint8_t *out_r,
                                          uint8_t *out_g,
                                          uint8_t *out_b) {
    float r = ((float)base_r) / 255.0f;
    float g = ((float)base_g) / 255.0f;
    float b = ((float)base_b) / 255.0f;

    if (white_mix > 0.0f) {
        r = datalab_overlay_color_lerp(r, 1.0f, white_mix);
        g = datalab_overlay_color_lerp(g, 1.0f, white_mix);
        b = datalab_overlay_color_lerp(b, 1.0f, white_mix);
    }
    if (black_mix > 0.0f) {
        r = datalab_overlay_color_lerp(r, 0.0f, black_mix);
        g = datalab_overlay_color_lerp(g, 0.0f, black_mix);
        b = datalab_overlay_color_lerp(b, 0.0f, black_mix);
    }

    if (out_r) *out_r = datalab_overlay_color_channel_from_unit(r);
    if (out_g) *out_g = datalab_overlay_color_channel_from_unit(g);
    if (out_b) *out_b = datalab_overlay_color_channel_from_unit(b);
}

static void datalab_overlay_ensure_text_contrast(const DatalabWorkspaceCustomTheme *theme,
                                                 uint8_t *io_text_r,
                                                 uint8_t *io_text_g,
                                                 uint8_t *io_text_b,
                                                 float min_ratio) {
    float ratio = 0.0f;
    float ratio_white = 0.0f;
    float ratio_black = 0.0f;

    if (!theme || !io_text_r || !io_text_g || !io_text_b) {
        return;
    }

    ratio = datalab_overlay_contrast_ratio_rgb(*io_text_r,
                                               *io_text_g,
                                               *io_text_b,
                                               theme->shell_fill_r,
                                               theme->shell_fill_g,
                                               theme->shell_fill_b);
    if (ratio >= min_ratio) {
        return;
    }

    ratio_white = datalab_overlay_contrast_ratio_rgb(255, 255, 255,
                                                     theme->shell_fill_r,
                                                     theme->shell_fill_g,
                                                     theme->shell_fill_b);
    ratio_black = datalab_overlay_contrast_ratio_rgb(0, 0, 0,
                                                     theme->shell_fill_r,
                                                     theme->shell_fill_g,
                                                     theme->shell_fill_b);
    if (ratio_white >= ratio_black) {
        *io_text_r = 255;
        *io_text_g = 255;
        *io_text_b = 255;
    } else {
        *io_text_r = 0;
        *io_text_g = 0;
        *io_text_b = 0;
    }
}

static void datalab_overlay_derive_custom_theme_from_base(uint8_t base_r,
                                                          uint8_t base_g,
                                                          uint8_t base_b,
                                                          DatalabWorkspaceCustomTheme *out_theme) {
    DatalabWorkspaceCustomTheme t = {0};
    float base_luma = datalab_overlay_relative_luminance(base_r, base_g, base_b);
    int dark_mode = base_luma < 0.48f;

    if (dark_mode) {
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.00f, 0.86f, &t.clear_r, &t.clear_g, &t.clear_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.02f, 0.64f, &t.pane_fill_r, &t.pane_fill_g, &t.pane_fill_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.01f, 0.72f, &t.shell_fill_r, &t.shell_fill_g, &t.shell_fill_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.36f, 0.08f, &t.shell_border_r, &t.shell_border_g, &t.shell_border_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.88f, 0.00f, &t.text_primary_r, &t.text_primary_g, &t.text_primary_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.70f, 0.00f, &t.text_secondary_r, &t.text_secondary_g, &t.text_secondary_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.06f, 0.58f, &t.button_fill_r, &t.button_fill_g, &t.button_fill_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.10f, 0.46f, &t.button_hover_r, &t.button_hover_g, &t.button_hover_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.22f, 0.24f, &t.button_active_r, &t.button_active_g, &t.button_active_b);
    } else {
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.92f, 0.00f, &t.clear_r, &t.clear_g, &t.clear_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.80f, 0.00f, &t.pane_fill_r, &t.pane_fill_g, &t.pane_fill_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.88f, 0.00f, &t.shell_fill_r, &t.shell_fill_g, &t.shell_fill_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.18f, 0.18f, &t.shell_border_r, &t.shell_border_g, &t.shell_border_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.02f, 0.86f, &t.text_primary_r, &t.text_primary_g, &t.text_primary_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.08f, 0.72f, &t.text_secondary_r, &t.text_secondary_g, &t.text_secondary_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.70f, 0.04f, &t.button_fill_r, &t.button_fill_g, &t.button_fill_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.60f, 0.04f, &t.button_hover_r, &t.button_hover_g, &t.button_hover_b);
        datalab_overlay_set_color_mix(base_r, base_g, base_b, 0.46f, 0.06f, &t.button_active_r, &t.button_active_g, &t.button_active_b);
    }

    datalab_overlay_ensure_text_contrast(&t, &t.text_primary_r, &t.text_primary_g, &t.text_primary_b, 4.5f);
    datalab_overlay_ensure_text_contrast(&t, &t.text_secondary_r, &t.text_secondary_g, &t.text_secondary_b, 3.0f);

    if (out_theme) {
        *out_theme = t;
    }
}

static int datalab_overlay_apply_custom_theme_assist(DatalabAppState *app_state) {
    DatalabWorkspaceCustomTheme *active_theme = NULL;
    const uint8_t *r_ptr = NULL;
    const uint8_t *g_ptr = NULL;
    const uint8_t *b_ptr = NULL;
    int token_index = 0;
    DatalabWorkspaceCustomTheme derived = {0};

    if (!app_state) {
        return 0;
    }
    active_theme = datalab_overlay_custom_theme_active_slot_ptr(app_state);
    if (!active_theme) {
        return 0;
    }
    token_index = datalab_overlay_custom_theme_token_clamp(
        (int)app_state->workspace_authoring_custom_theme_selected_token);
    r_ptr = datalab_overlay_custom_theme_channel_ptr_const(active_theme, token_index, 0);
    g_ptr = datalab_overlay_custom_theme_channel_ptr_const(active_theme, token_index, 1);
    b_ptr = datalab_overlay_custom_theme_channel_ptr_const(active_theme, token_index, 2);
    if (!r_ptr || !g_ptr || !b_ptr) {
        return 0;
    }

    datalab_overlay_derive_custom_theme_from_base(*r_ptr, *g_ptr, *b_ptr, &derived);
    *active_theme = derived;
    app_state->workspace_authoring_custom_theme = *active_theme;
    app_state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    app_state->workspace_authoring_pending_stub = 1u;
    return 1;
}

static int datalab_overlay_apply_custom_theme_delta(DatalabAppState *app_state, int delta) {
    uint8_t *channel_ptr = NULL;
    DatalabWorkspaceCustomTheme *active_theme = NULL;
    int value = 0;
    int token_index = 0;
    int channel_index = 0;

    if (!app_state || delta == 0) {
        return 0;
    }

    token_index = datalab_overlay_custom_theme_token_clamp(
        (int)app_state->workspace_authoring_custom_theme_selected_token);
    channel_index = datalab_overlay_custom_theme_channel_clamp(
        (int)app_state->workspace_authoring_custom_theme_selected_channel);
    app_state->workspace_authoring_custom_theme_selected_token = (uint8_t)token_index;
    app_state->workspace_authoring_custom_theme_selected_channel = (uint8_t)channel_index;
    active_theme = datalab_overlay_custom_theme_active_slot_ptr(app_state);
    if (!active_theme) {
        return 0;
    }

    channel_ptr = datalab_overlay_custom_theme_channel_ptr(active_theme,
                                                           token_index,
                                                           channel_index);
    if (!channel_ptr) {
        return 0;
    }

    value = (int)(*channel_ptr) + delta;
    value = datalab_overlay_clamp_int(value, 0, 255);
    if (value == (int)(*channel_ptr)) {
        return 0;
    }
    *channel_ptr = (uint8_t)value;
    app_state->workspace_authoring_custom_theme = *active_theme;
    app_state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    app_state->workspace_authoring_pending_stub = 1u;
    return 1;
}

static int datalab_overlay_open_custom_editor(DatalabAppState *app_state, int force_custom_preset) {
    int changed = 0;

    if (!app_state) {
        return 0;
    }
    datalab_overlay_sync_custom_theme_from_active_slot(app_state);
    if (force_custom_preset &&
        app_state->workspace_authoring_theme_preset_id != (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        app_state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
        changed = 1;
    }
    if (!app_state->workspace_authoring_custom_theme_popup_open) {
        app_state->workspace_authoring_custom_theme_popup_open = 1u;
    }
    if (changed) {
        app_state->workspace_authoring_pending_stub = 1u;
    }
    return 1;
}

static int datalab_overlay_create_custom_theme_from_active_preset(DatalabAppState *app_state) {
    DatalabWorkspaceAuthoringThemePreset selected_theme = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    DatalabAuthoringThemePalette seed_palette = {0};
    DatalabWorkspaceCustomTheme seeded_theme = {0};
    DatalabWorkspaceCustomTheme *active_theme = NULL;
    int changed = 0;

    if (!app_state) {
        return 0;
    }
    selected_theme = datalab_overlay_selected_theme(app_state);
    datalab_overlay_theme_palette(selected_theme,
                                  &app_state->workspace_authoring_custom_theme,
                                  &seed_palette);
    seeded_theme = datalab_overlay_custom_theme_from_palette(&seed_palette);
    active_theme = datalab_overlay_custom_theme_active_slot_ptr(app_state);
    if (!active_theme) {
        return 0;
    }
    if (memcmp(active_theme, &seeded_theme, sizeof(seeded_theme)) != 0) {
        *active_theme = seeded_theme;
        app_state->workspace_authoring_custom_theme = *active_theme;
        changed = 1;
    }
    app_state->workspace_authoring_custom_theme_selected_token = 0u;
    app_state->workspace_authoring_custom_theme_selected_channel = 0u;
    if (app_state->workspace_authoring_theme_preset_id != (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        app_state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
        changed = 1;
    }
    if (!app_state->workspace_authoring_custom_theme_popup_open) {
        app_state->workspace_authoring_custom_theme_popup_open = 1u;
    }
    if (changed) {
        app_state->workspace_authoring_pending_stub = 1u;
    }
    return 1;
}

static int datalab_overlay_select_custom_theme_slot(DatalabAppState *app_state, int slot_index) {
    int clamped_slot = 0;
    int changed = 0;
    if (!app_state) {
        return 0;
    }
    clamped_slot = datalab_overlay_custom_theme_slot_clamp(slot_index);
    if ((int)app_state->workspace_authoring_custom_theme_active_slot != clamped_slot) {
        app_state->workspace_authoring_custom_theme_active_slot = (uint8_t)clamped_slot;
        changed = 1;
    }
    datalab_overlay_sync_custom_theme_from_active_slot(app_state);
    if (app_state->workspace_authoring_theme_preset_id != (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        app_state->workspace_authoring_theme_preset_id = (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
        changed = 1;
    }
    if (changed) {
        app_state->workspace_authoring_pending_stub = 1u;
    }
    return 1;
}

static int datalab_overlay_cycle_custom_theme_slot_name(DatalabAppState *app_state) {
    static const char *k_name_catalog[] = {
        "custom_1",
        "custom_2",
        "custom_3",
        "aurora",
        "ember",
        "mint",
        "graphite",
        "ocean",
        "sunset"
    };
    int slot_index = 0;
    size_t i = 0u;
    size_t next = 0u;
    size_t catalog_count = sizeof(k_name_catalog) / sizeof(k_name_catalog[0]);
    const char *current = NULL;
    if (!app_state || catalog_count == 0u) {
        return 0;
    }
    slot_index = datalab_overlay_custom_theme_slot_clamp((int)app_state->workspace_authoring_custom_theme_active_slot);
    current = app_state->workspace_authoring_custom_theme_slot_names[slot_index];
    for (i = 0u; i < catalog_count; ++i) {
        if (strcmp(current, k_name_catalog[i]) == 0) {
            next = (i + 1u) % catalog_count;
            break;
        }
    }
    (void)snprintf(app_state->workspace_authoring_custom_theme_slot_names[slot_index],
                   DATALAB_CUSTOM_THEME_NAME_CAP,
                   "%s",
                   k_name_catalog[next]);
    app_state->workspace_authoring_pending_stub = 1u;
    return 1;
}

static int datalab_overlay_pane_mode_active(const DatalabAppState *app_state) {
    int layout_mode = DATALAB_AUTHORING_LAYOUT_MODE_RUNTIME;
    int overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;

    if (!app_state) {
        return 1;
    }
    if (app_state->workspace_authoring_stub_active) {
        layout_mode = DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING;
    }
    overlay_mode = (int)app_state->workspace_authoring_overlay_mode;
    return kit_workspace_authoring_ui_pane_overlay_visible(layout_mode,
                                                           DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING,
                                                           overlay_mode,
                                                           DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME);
}

static void datalab_overlay_draw_centered_text(SDL_Renderer *renderer,
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

static void datalab_overlay_draw_button(SDL_Renderer *renderer,
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

static void datalab_overlay_capture_top_buttons(int viewport_width, int pane_overlay_active) {
    memset(g_datalab_authoring_overlay_ui.top_buttons, 0, sizeof(g_datalab_authoring_overlay_ui.top_buttons));
    g_datalab_authoring_overlay_ui.top_button_count =
        kit_workspace_authoring_ui_build_overlay_buttons(viewport_width,
                                                         1,
                                                         pane_overlay_active,
                                                         g_datalab_authoring_overlay_ui.top_buttons,
                                                         8u);
    g_datalab_authoring_overlay_ui.pane_overlay_active = pane_overlay_active;
}

static void datalab_overlay_draw_top_buttons(SDL_Renderer *renderer, const DatalabAuthoringThemePalette *palette) {
    uint32_t i;

    if (!renderer || !palette) {
        return;
    }

    for (i = 0u; i < g_datalab_authoring_overlay_ui.top_button_count; ++i) {
        const KitWorkspaceAuthoringOverlayButton *button = &g_datalab_authoring_overlay_ui.top_buttons[i];
        SDL_Rect rect;
        if (!button->visible) {
            continue;
        }
        rect = datalab_overlay_rect_from_core(button->rect);
        datalab_overlay_draw_button(renderer,
                                    &rect,
                                    button->label,
                                    g_datalab_authoring_overlay_ui.hover_top_button == button->id,
                                    0,
                                    palette);
    }
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

static int datalab_overlay_apply_font_theme_hit(DatalabAuthoringFontThemeHitId hit_id, DatalabAppState *app_state) {
    int next_step = 0;
    int changed = 0;

    if (!app_state) {
        return 0;
    }

    next_step = app_state->text_zoom_step;
    switch (hit_id) {
        case DATALAB_AUTHORING_FONT_HIT_TEXT_DEC:
            next_step = datalab_text_zoom_step_clamp(app_state->text_zoom_step - 1);
            if (next_step != app_state->text_zoom_step) {
                app_state->text_zoom_step = next_step;
                datalab_set_text_zoom_step(next_step);
                changed = 1;
            }
            break;
        case DATALAB_AUTHORING_FONT_HIT_TEXT_INC:
            next_step = datalab_text_zoom_step_clamp(app_state->text_zoom_step + 1);
            if (next_step != app_state->text_zoom_step) {
                app_state->text_zoom_step = next_step;
                datalab_set_text_zoom_step(next_step);
                changed = 1;
            }
            break;
        case DATALAB_AUTHORING_FONT_HIT_TEXT_RESET:
            if (app_state->text_zoom_step != 0) {
                app_state->text_zoom_step = 0;
                datalab_set_text_zoom_step(0);
                changed = 1;
            }
            break;
        case DATALAB_AUTHORING_FONT_HIT_THEME_0:
        case DATALAB_AUTHORING_FONT_HIT_THEME_1:
        case DATALAB_AUTHORING_FONT_HIT_THEME_2:
        case DATALAB_AUTHORING_FONT_HIT_THEME_3:
        case DATALAB_AUTHORING_FONT_HIT_THEME_4:
        case DATALAB_AUTHORING_FONT_HIT_THEME_5: {
            uint8_t next_theme =
                (uint8_t)datalab_overlay_theme_preset_clamp((int)(hit_id - DATALAB_AUTHORING_FONT_HIT_THEME_0));
            if (app_state->workspace_authoring_theme_preset_id != next_theme) {
                app_state->workspace_authoring_theme_preset_id = next_theme;
                changed = 1;
            }
            break;
        }
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_OPEN:
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_EDIT:
            return datalab_overlay_open_custom_editor(app_state, 1);
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_CREATE:
            return datalab_overlay_create_custom_theme_from_active_preset(app_state);
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_0:
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_1:
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_2:
            return datalab_overlay_select_custom_theme_slot(
                app_state,
                (int)(hit_id - DATALAB_AUTHORING_FONT_HIT_CUSTOM_SLOT_0));
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_RENAME:
            return datalab_overlay_cycle_custom_theme_slot_name(app_state);
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_POPUP_CLOSE:
            if (app_state->workspace_authoring_custom_theme_popup_open) {
                app_state->workspace_authoring_custom_theme_popup_open = 0u;
            }
            return 1;
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0:
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_8:
            break;
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_R:
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_G:
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_B:
            app_state->workspace_authoring_custom_theme_selected_channel =
                (uint8_t)datalab_overlay_custom_theme_channel_clamp(
                    (int)(hit_id - DATALAB_AUTHORING_FONT_HIT_CUSTOM_CHANNEL_R));
            return 1;
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_10:
            return datalab_overlay_apply_custom_theme_delta(app_state, -10);
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_DEC_1:
            return datalab_overlay_apply_custom_theme_delta(app_state, -1);
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_INC_1:
            return datalab_overlay_apply_custom_theme_delta(app_state, 1);
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_INC_10:
            return datalab_overlay_apply_custom_theme_delta(app_state, 10);
        case DATALAB_AUTHORING_FONT_HIT_CUSTOM_ASSIST:
            return datalab_overlay_apply_custom_theme_assist(app_state);
        case DATALAB_AUTHORING_FONT_HIT_NONE:
        default:
            break;
    }

    if (hit_id >= DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0 &&
        hit_id <= DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_8) {
        app_state->workspace_authoring_custom_theme_selected_token =
            (uint8_t)datalab_overlay_custom_theme_token_clamp(
                (int)(hit_id - DATALAB_AUTHORING_FONT_HIT_CUSTOM_TOKEN_0));
        return 1;
    }

    if (changed) {
        app_state->workspace_authoring_pending_stub = 1u;
    }
    return changed;
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

static void datalab_overlay_draw_pane_takeover(SDL_Renderer *renderer,
                                               const DatalabFrame *frame,
                                               const DatalabAppState *app_state,
                                               const DatalabAuthoringThemePalette *palette,
                                               int ww,
                                               int wh) {
    SDL_Rect pane = {0};
    SDL_Rect header = {0};
    char status_line[192];
    int pad = 0;

    if (!renderer || !app_state || !palette || ww <= 0 || wh <= 0) {
        return;
    }

    pad = datalab_scaled_px(12.0f);
    pane.x = 0;
    pane.y = datalab_scaled_px(34.0f);
    pane.w = ww;
    pane.h = wh - pane.y;
    if (pane.h < datalab_scaled_px(50.0f)) {
        pane.h = datalab_scaled_px(50.0f);
    }
    header = pane;
    header.h = datalab_scaled_px(24.0f);

    SDL_SetRenderDrawColor(renderer, palette->clear_r, palette->clear_g, palette->clear_b, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, palette->pane_fill_r, palette->pane_fill_g, palette->pane_fill_b, 255);
    SDL_RenderFillRect(renderer, &pane);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
    SDL_RenderDrawRect(renderer, &pane);
    SDL_SetRenderDrawColor(renderer, palette->shell_fill_r, palette->shell_fill_g, palette->shell_fill_b, 255);
    SDL_RenderFillRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
    SDL_RenderDrawRect(renderer, &header);

    draw_text_5x7(renderer,
                  header.x + pad,
                  header.y + datalab_scaled_px(6.0f),
                  "Pane 1 [DataLab Host]",
                  1,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);
    snprintf(status_line,
             sizeof(status_line),
             "profile:%s pending:%u apply:%u cancel:%u theme:%s",
             datalab_overlay_profile_name(frame),
             (unsigned int)app_state->workspace_authoring_pending_stub,
             (unsigned int)app_state->workspace_authoring_apply_count,
             (unsigned int)app_state->workspace_authoring_cancel_count,
             datalab_overlay_theme_name(datalab_overlay_selected_theme(app_state)));
    draw_text_5x7(renderer,
                  pane.x + pad,
                  pane.y + datalab_scaled_px(38.0f),
                  status_line,
                  1,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);
    datalab_overlay_draw_centered_text(renderer,
                                       &pane,
                                       pane.y + (pane.h / 2),
                                       "DataLab Authoring Overlay (Pane)",
                                       palette->text_primary_r,
                                       palette->text_primary_g,
                                       palette->text_primary_b,
                                       255);
    g_datalab_authoring_overlay_ui.font_controls_valid = 0u;
    g_datalab_authoring_overlay_ui.hover_font_hit = DATALAB_AUTHORING_FONT_HIT_NONE;
}

static void datalab_overlay_draw_font_theme_takeover(SDL_Renderer *renderer,
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

static CoreResult datalab_workspace_authoring_draw_scene_callback(
    void *host_context,
    const KitWorkspaceAuthoringRenderDeriveFrame *derive) {
    DatalabAuthoringOverlayHost *host = (DatalabAuthoringOverlayHost *)host_context;
    DatalabAuthoringThemePalette palette = {0};
    DatalabWorkspaceAuthoringThemePreset selected_theme = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    int pane_overlay_active = 1;
    SDL_Rect help_strip = {0};

    if (!host || !host->renderer || !host->frame || !host->app_state || !derive) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring overlay draw request" };
    }

    selected_theme = datalab_overlay_selected_theme(host->app_state);
    datalab_overlay_theme_palette(selected_theme,
                                  &host->app_state->workspace_authoring_custom_theme,
                                  &palette);

    pane_overlay_active = datalab_overlay_pane_mode_active(host->app_state);
    g_datalab_authoring_overlay_ui.viewport_w = derive->width;
    g_datalab_authoring_overlay_ui.viewport_h = derive->height;
    datalab_overlay_capture_top_buttons(derive->width, pane_overlay_active);

    if (pane_overlay_active) {
        datalab_overlay_draw_pane_takeover(host->renderer,
                                           host->frame,
                                           host->app_state,
                                           &palette,
                                           derive->width,
                                           derive->height);
    } else {
        datalab_overlay_draw_font_theme_takeover(host->renderer,
                                                 host->app_state,
                                                 &palette,
                                                 derive->width,
                                                 derive->height);
    }

    datalab_overlay_draw_top_buttons(host->renderer, &palette);

    help_strip.x = datalab_scaled_px(8.0f);
    help_strip.y = datalab_scaled_px(30.0f);
    help_strip.w = datalab_overlay_clamp_int(derive->width - datalab_scaled_px(16.0f), 20, derive->width);
    help_strip.h = datalab_scaled_px(18.0f);
    SDL_SetRenderDrawBlendMode(host->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(host->renderer, palette.clear_r, palette.clear_g, palette.clear_b, 210);
    SDL_RenderFillRect(host->renderer, &help_strip);
    SDL_SetRenderDrawColor(host->renderer,
                           palette.shell_border_r,
                           palette.shell_border_g,
                           palette.shell_border_b,
                           238);
    SDL_RenderDrawRect(host->renderer, &help_strip);
    draw_text_5x7(host->renderer,
                  help_strip.x + datalab_scaled_px(6.0f),
                  help_strip.y + datalab_scaled_px(4.0f),
                  "ALT+C+V toggle | TAB overlay | ENTER apply | ESC cancel | mouse: theme/text controls",
                  1,
                  palette.text_secondary_r,
                  palette.text_secondary_g,
                  palette.text_secondary_b,
                  255);
    return core_result_ok();
}

void datalab_workspace_authoring_submit_takeover(SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 const DatalabFrame *frame,
                                                 const DatalabAppState *app_state,
                                                 const char *title,
                                                 DatalabRenderSubmitOutcome *outcome) {
    DatalabAuthoringOverlayHost host = {0};
    KitWorkspaceAuthoringRenderDeriveFrame shared_derive = {0};
    KitWorkspaceAuthoringRenderSubmitOutcome shared_outcome = {0};
    int ww = 0;
    int wh = 0;

    if (!outcome) {
        return;
    }
    memset(outcome, 0, sizeof(*outcome));
    if (!window || !renderer || !frame || !app_state) {
        outcome->result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring takeover request" };
        return;
    }

    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    if (ww <= 0 || wh <= 0) {
        outcome->result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring viewport" };
        return;
    }

    kit_workspace_authoring_ui_derive_frame(&shared_derive,
                                            ww,
                                            wh,
                                            1,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0.0f,
                                            0.0f,
                                            0.0f,
                                            0u,
                                            0u,
                                            0u,
                                            (int)(frame->frame_index & 0x7fffffffULL),
                                            (int)app_state->workspace_authoring_pending_stub);

    host.renderer = renderer;
    host.frame = frame;
    host.app_state = app_state;

    kit_workspace_authoring_ui_submit_frame(&host,
                                            &shared_derive,
                                            datalab_workspace_authoring_draw_scene_callback,
                                            NULL,
                                            NULL,
                                            &shared_outcome);
    outcome->result = shared_outcome.draw_result;
    if (outcome->result.code != CORE_OK) {
        return;
    }

    SDL_SetWindowTitle(window, (title && title[0]) ? title : "DataLab");
    SDL_RenderPresent(renderer);
    outcome->presented = 1u;
}
