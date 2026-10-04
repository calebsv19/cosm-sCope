#include "render/render_view_ui_buttons.h"

static int datalab_ui_measure_text(void *user, const char *text, int scale,
                                   int *out_width, int *out_height) {
    (void)user;
    return datalab_measure_text(scale, text, out_width, out_height);
}

static int datalab_ui_line_height(void *user, int scale) {
    (void)user;
    return datalab_text_line_height(scale);
}

static void datalab_ui_draw_text(void *user, SDL_Renderer *renderer,
                                 const SDL_Rect *clip_rect, int x, int y,
                                 const char *text, int scale, KitRenderColor color) {
    (void)user;
    draw_text_5x7_clipped(renderer, clip_rect, x, y, text, scale,
                          color.r, color.g, color.b, color.a);
}

KitUiSdlTextApi datalab_ui_text_api(int clip_padding_x) {
    /* The host font already applies text zoom. Do not multiply its scale again. */
    return (KitUiSdlTextApi){NULL, 1, clip_padding_x,
                             datalab_ui_measure_text, datalab_ui_line_height,
                             datalab_ui_draw_text};
}

void datalab_ui_draw_button(SDL_Renderer *renderer,
                            const SDL_Rect *rect,
                            const char *label,
                            const KitUiButtonState *state,
                            const DatalabAuthoringThemePalette *palette) {
    KitUiButtonSpec spec;
    KitUiButtonAppearance appearance;
    KitUiButtonTheme theme;
    KitUiSdlTextApi text_api;

    if (!renderer || !rect || !label || !palette) {
        return;
    }
    kit_ui_button_spec_init(&spec, label);
    if (state) {
        spec.state = *state;
    }
    (void)kit_ui_button_appearance_preset(KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED,
                                          &appearance);
    appearance.corner_radius = (float)datalab_scaled_px(appearance.corner_radius);
    appearance.border_thickness = (float)datalab_scaled_px(appearance.border_thickness);
    theme = (KitUiButtonTheme){
        .idle_fill = {palette->button_fill_r, palette->button_fill_g, palette->button_fill_b, 255},
        .selected_fill = {palette->button_active_r, palette->button_active_g, palette->button_active_b, 255},
        .hover_fill = {palette->button_hover_r, palette->button_hover_g, palette->button_hover_b, 255},
        .positive_fill = {palette->button_active_r, palette->button_active_g, palette->button_active_b, 255},
        .outline_idle = {palette->text_secondary_r, palette->text_secondary_g, palette->text_secondary_b, 255},
        .outline_highlight = {palette->button_active_r, palette->button_active_g, palette->button_active_b, 255},
        .text_primary = {palette->text_primary_r, palette->text_primary_g, palette->text_primary_b, 255},
        .text_muted = {palette->text_secondary_r, palette->text_secondary_g, palette->text_secondary_b, 255}
    };
    text_api = datalab_ui_text_api(datalab_scaled_px(4.0f));
    kit_ui_sdl_draw_button_spec_appearance(renderer, rect, &spec, &theme, &appearance, &text_api);
}

/* Existing authoring/session callers expose hover/selection, while playback
 * provides the complete state. Both expressions use the same adapter above. */
void datalab_overlay_draw_button(SDL_Renderer *renderer,
                                 const SDL_Rect *rect,
                                 const char *label,
                                 int hover,
                                 int active,
                                 const DatalabAuthoringThemePalette *palette) {
    KitUiButtonState state;
    kit_ui_button_state_init(&state);
    state.hovered = hover != 0;
    state.selected = active != 0;
    datalab_ui_draw_button(renderer, rect, label, &state, palette);
}
