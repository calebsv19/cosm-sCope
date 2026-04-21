#include "render/render_view_authoring_overlay_shared.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static DatalabWorkspaceAuthoringThemePreset datalab_overlay_theme_preset_clamp(int value) {
    if (value < (int)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT) {
        return DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT;
    }
    if (value > (int)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        return DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    }
    return (DatalabWorkspaceAuthoringThemePreset)value;
}

const char *datalab_overlay_theme_name(DatalabWorkspaceAuthoringThemePreset preset) {
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

void datalab_overlay_theme_palette(DatalabWorkspaceAuthoringThemePreset preset,
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

DatalabWorkspaceAuthoringThemePreset datalab_overlay_selected_theme(const DatalabAppState *app_state) {
    if (!app_state) {
        return DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    }
    return datalab_overlay_theme_preset_clamp((int)app_state->workspace_authoring_theme_preset_id);
}

int datalab_overlay_custom_theme_token_clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value >= DATALAB_CUSTOM_THEME_TOKEN_COUNT) {
        return DATALAB_CUSTOM_THEME_TOKEN_COUNT - 1;
    }
    return value;
}

int datalab_overlay_custom_theme_channel_clamp(int value) {
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

const uint8_t *datalab_overlay_custom_theme_channel_ptr_const(const DatalabWorkspaceCustomTheme *theme,
                                                              int token_index,
                                                              int channel_index) {
    return datalab_overlay_custom_theme_channel_ptr((DatalabWorkspaceCustomTheme *)theme,
                                                    token_index,
                                                    channel_index);
}

int datalab_overlay_custom_theme_slot_clamp(int value) {
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

const DatalabWorkspaceCustomTheme *datalab_overlay_custom_theme_active_slot_ptr_const(
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

int datalab_overlay_apply_font_theme_hit(DatalabAuthoringFontThemeHitId hit_id, DatalabAppState *app_state) {
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
