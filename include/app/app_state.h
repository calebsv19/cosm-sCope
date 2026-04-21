#ifndef DATALAB_APP_STATE_H
#define DATALAB_APP_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "data/pack_loader.h"

#define DATALAB_APP_PATH_CAP 1024
#define DATALAB_CUSTOM_THEME_SLOT_COUNT 3
#define DATALAB_CUSTOM_THEME_NAME_CAP 24

typedef enum DatalabViewMode {
    DATALAB_VIEW_DENSITY = 1,
    DATALAB_VIEW_SPEED = 2,
    DATALAB_VIEW_DENSITY_VECTOR = 3
} DatalabViewMode;

typedef enum DatalabWorkspaceAuthoringOverlayMode {
    DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE = 0,
    DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME = 1
} DatalabWorkspaceAuthoringOverlayMode;

typedef enum DatalabWorkspaceAuthoringThemePreset {
    DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT = 0,
    DATALAB_WORKSPACE_AUTHORING_THEME_STANDARD_GREY = 1,
    DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST = 2,
    DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT = 3,
    DATALAB_WORKSPACE_AUTHORING_THEME_GREYSCALE = 4,
    DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM = 5
} DatalabWorkspaceAuthoringThemePreset;

typedef struct DatalabWorkspaceCustomTheme {
    uint8_t clear_r, clear_g, clear_b;
    uint8_t pane_fill_r, pane_fill_g, pane_fill_b;
    uint8_t shell_fill_r, shell_fill_g, shell_fill_b;
    uint8_t shell_border_r, shell_border_g, shell_border_b;
    uint8_t text_primary_r, text_primary_g, text_primary_b;
    uint8_t text_secondary_r, text_secondary_g, text_secondary_b;
    uint8_t button_fill_r, button_fill_g, button_fill_b;
    uint8_t button_hover_r, button_hover_g, button_hover_b;
    uint8_t button_active_r, button_active_g, button_active_b;
} DatalabWorkspaceCustomTheme;

#define DATALAB_TEXT_ZOOM_STEP_MIN (-4)
#define DATALAB_TEXT_ZOOM_STEP_MAX 5

typedef struct DatalabAppState {
    const char *pack_path;
    DatalabProfile profile;
    char input_root[DATALAB_APP_PATH_CAP];
    DatalabViewMode view_mode;
    int text_zoom_step;
    uint32_t vector_stride;
    float vector_scale;
    size_t trace_cursor_index;
    float trace_zoom_stub;
    int trace_selection_stub_active;
    size_t trace_lane_visibility_index; /* 0 = all, 1..N = one lane by index */
    int trace_lane_cycle_requested;
    int open_picker_requested;
    int panel_rescan_requested;
    int panel_selection_delta;
    size_t panel_selected_index;
    int panel_open_selected_requested;
    char panel_requested_pack_path[DATALAB_APP_PATH_CAP];
    int workspace_authoring_stub_active;
    uint8_t workspace_authoring_entry_chord_mask;
    uint32_t workspace_authoring_entry_count;
    DatalabWorkspaceAuthoringOverlayMode workspace_authoring_overlay_mode;
    uint8_t workspace_authoring_pending_stub;
    uint8_t workspace_authoring_theme_preset_id;
    DatalabWorkspaceCustomTheme workspace_authoring_custom_theme;
    uint8_t workspace_authoring_custom_theme_active_slot;
    DatalabWorkspaceCustomTheme workspace_authoring_custom_theme_slots[DATALAB_CUSTOM_THEME_SLOT_COUNT];
    char workspace_authoring_custom_theme_slot_names[DATALAB_CUSTOM_THEME_SLOT_COUNT][DATALAB_CUSTOM_THEME_NAME_CAP];
    uint8_t workspace_authoring_custom_theme_popup_open;
    uint8_t workspace_authoring_custom_theme_selected_token;
    uint8_t workspace_authoring_custom_theme_selected_channel;
    int workspace_authoring_entry_text_zoom_step;
    uint8_t workspace_authoring_entry_theme_preset_id;
    DatalabWorkspaceCustomTheme workspace_authoring_entry_custom_theme;
    uint8_t workspace_authoring_entry_custom_theme_active_slot;
    DatalabWorkspaceCustomTheme workspace_authoring_entry_custom_theme_slots[DATALAB_CUSTOM_THEME_SLOT_COUNT];
    char workspace_authoring_entry_custom_theme_slot_names[DATALAB_CUSTOM_THEME_SLOT_COUNT][DATALAB_CUSTOM_THEME_NAME_CAP];
    uint8_t workspace_authoring_entry_custom_theme_selected_token;
    uint8_t workspace_authoring_entry_custom_theme_selected_channel;
    uint32_t workspace_authoring_overlay_cycle_count;
    uint32_t workspace_authoring_apply_count;
    uint32_t workspace_authoring_cancel_count;
} DatalabAppState;

void datalab_app_state_init(DatalabAppState *state, const char *pack_path, DatalabProfile profile);
const char *datalab_view_mode_name(DatalabViewMode mode);
const char *datalab_workspace_authoring_overlay_mode_name(DatalabWorkspaceAuthoringOverlayMode mode);
int datalab_text_zoom_step_clamp(int step);
float datalab_text_zoom_step_multiplier(int step);

#endif
