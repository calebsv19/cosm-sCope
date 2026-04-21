#ifndef DATALAB_RENDER_VIEW_AUTHORING_OVERLAY_SHARED_H
#define DATALAB_RENDER_VIEW_AUTHORING_OVERLAY_SHARED_H

#include "render/render_view_internal.h"
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

extern DatalabAuthoringOverlayUiState g_datalab_authoring_overlay_ui;

int datalab_overlay_clamp_int(int value, int min_value, int max_value);
void datalab_overlay_draw_centered_text(SDL_Renderer *renderer,
                                        const SDL_Rect *rect,
                                        int y,
                                        const char *text,
                                        uint8_t r,
                                        uint8_t g,
                                        uint8_t b,
                                        uint8_t a);
void datalab_overlay_draw_button(SDL_Renderer *renderer,
                                 const SDL_Rect *rect,
                                 const char *label,
                                 int hover,
                                 int active,
                                 const DatalabAuthoringThemePalette *palette);
void datalab_overlay_draw_font_theme_takeover(SDL_Renderer *renderer,
                                              const DatalabAppState *app_state,
                                              const DatalabAuthoringThemePalette *palette,
                                              int ww,
                                              int wh);
const char *datalab_overlay_theme_name(DatalabWorkspaceAuthoringThemePreset preset);
void datalab_overlay_theme_palette(DatalabWorkspaceAuthoringThemePreset preset,
                                   const DatalabWorkspaceCustomTheme *custom_theme,
                                   DatalabAuthoringThemePalette *out_palette);
DatalabWorkspaceAuthoringThemePreset datalab_overlay_selected_theme(const DatalabAppState *app_state);
int datalab_overlay_custom_theme_token_clamp(int value);
int datalab_overlay_custom_theme_channel_clamp(int value);
int datalab_overlay_custom_theme_slot_clamp(int value);
const DatalabWorkspaceCustomTheme *datalab_overlay_custom_theme_active_slot_ptr_const(
    const DatalabAppState *app_state);
const uint8_t *datalab_overlay_custom_theme_channel_ptr_const(const DatalabWorkspaceCustomTheme *theme,
                                                              int token_index,
                                                              int channel_index);
int datalab_overlay_apply_font_theme_hit(DatalabAuthoringFontThemeHitId hit_id, DatalabAppState *app_state);

#endif
