#ifndef DATALAB_APP_STATE_H
#define DATALAB_APP_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "core_viewport2d.h"
#include "core_pane.h"
#include "core_workspace_authoring_session.h"
#include "data/pack_loader.h"

struct DatalabInputCatalog;
struct DatalabAppRuntime;
struct DatalabAsyncDecode;

#define DATALAB_APP_PATH_CAP 1024
#define DATALAB_RECENT_INPUT_ROOT_LIMIT 48
#define DATALAB_RECENT_INPUT_FILE_LIMIT 64
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

typedef enum DatalabPlaybackMode {
    DATALAB_PLAYBACK_MODE_LOOP = 0,
    DATALAB_PLAYBACK_MODE_BOUNCE = 1
} DatalabPlaybackMode;

typedef enum DatalabSamplingMode {
    DATALAB_SAMPLING_MODE_DEFAULT = 0,
    DATALAB_SAMPLING_MODE_NEAREST = 1,
    DATALAB_SAMPLING_MODE_LINEAR = 2
} DatalabSamplingMode;

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

typedef struct DatalabRasterViewportState {
    CoreViewport2D viewport;
    uint32_t content_width;
    uint32_t content_height;
    int view_width;
    int view_height;
    int valid;
    int fit_mode;
    int reset_requested;
    int drag_active;
    int last_mouse_x;
    int last_mouse_y;
} DatalabRasterViewportState;

enum {
    DATALAB_WORKSPACE_PROJECTION_NODE_COUNT = 3,
    DATALAB_WORKSPACE_SURFACE_PROFILE = 1u,
    DATALAB_WORKSPACE_SURFACE_SOURCE_CONTROLS = 2u
};

typedef struct DatalabWorkspaceAuthoringProjection {
    CorePaneNode nodes[DATALAB_WORKSPACE_PROJECTION_NODE_COUNT];
    float profile_surface_ratio;
} DatalabWorkspaceAuthoringProjection;

#define DATALAB_TEXT_ZOOM_STEP_MIN (-4)
#define DATALAB_TEXT_ZOOM_STEP_MAX 5
#define DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT 120u
#define DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT 2
#define DATALAB_PLAYBACK_SPEED_INDEX_MIN 0
#define DATALAB_PLAYBACK_SPEED_INDEX_MAX 4

typedef struct DatalabAppState {
    const char *pack_path;
    DatalabProfile profile;
    struct DatalabInputCatalog *input_catalog;
    struct DatalabAppRuntime *runtime_owner;
    struct DatalabAsyncDecode *async_decode;
    int async_decode_frame_ready;
    char input_root[DATALAB_APP_PATH_CAP];
    char recent_input_roots[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP];
    size_t recent_input_root_count;
    int recent_input_root_dropdown_open;
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
    int panel_selection_home_requested;
    int panel_selection_end_requested;
    size_t panel_selected_index;
    int panel_open_selected_requested;
    char panel_requested_pack_path[DATALAB_APP_PATH_CAP];
    int playback_active;
    DatalabPlaybackMode playback_mode;
    int playback_direction;
    int playback_speed_index;
    uint32_t playback_interval_ms;
    uint32_t playback_last_advance_ticks;
    int session_hud_collapsed;
    DatalabSamplingMode sampling_mode;
    int raster_actual_pixel_mode;
    int raster_alpha_checkerboard;
    int raster_probe_valid;
    uint32_t raster_probe_x;
    uint32_t raster_probe_y;
    DatalabRasterViewportState raster_viewport;
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
    DatalabWorkspaceAuthoringProjection workspace_authoring_projection;
    DatalabWorkspaceAuthoringProjection workspace_authoring_entry_projection;
    CoreWorkspaceAuthoringSession workspace_authoring_session;
} DatalabAppState;

void datalab_app_state_init(DatalabAppState *state, const char *pack_path, DatalabProfile profile);
const char *datalab_view_mode_name(DatalabViewMode mode);
const char *datalab_workspace_authoring_overlay_mode_name(DatalabWorkspaceAuthoringOverlayMode mode);
int datalab_text_zoom_step_clamp(int step);
float datalab_text_zoom_step_multiplier(int step);
int datalab_playback_speed_index_clamp(int speed_index);
uint32_t datalab_playback_interval_for_speed_index(int speed_index);
void datalab_panel_request_step(DatalabAppState *state, int delta, int open_selected);
void datalab_panel_request_home(DatalabAppState *state, int open_selected);
void datalab_panel_request_end(DatalabAppState *state, int open_selected);
void datalab_panel_request_open_selected(DatalabAppState *state);
void datalab_panel_clear_request(DatalabAppState *state);
void datalab_panel_request_pack_path(DatalabAppState *state, const char *path);
int datalab_input_root_join_child_file(const char *root,
                                       const char *file_name,
                                       char *out_path,
                                       size_t out_cap);
int datalab_panel_request_pack_under_root(DatalabAppState *state, const char *root, const char *file_name);
int datalab_panel_consume_requested_pack_path(DatalabAppState *state, char *out_path, size_t out_cap);
void datalab_panel_reset_interaction_state(DatalabAppState *state);
void datalab_playback_toggle_active(DatalabAppState *state, uint32_t now_ticks, uint32_t fallback_interval_ms);
void datalab_playback_set_speed_index(DatalabAppState *state, int speed_index, uint32_t now_ticks);
void datalab_playback_set_mode(DatalabAppState *state, DatalabPlaybackMode mode);
void datalab_playback_stop(DatalabAppState *state);
int datalab_app_state_select_input_root(DatalabAppState *state, const char *path);
void datalab_app_state_request_picker(DatalabAppState *state);
void datalab_app_state_request_panel_rescan(DatalabAppState *state);
void datalab_app_state_reset_interactions(DatalabAppState *state);
void datalab_profile_select_view_slot(DatalabAppState *state, int slot);
void datalab_physics_adjust_vector_stride(DatalabAppState *state, int delta);
void datalab_trace_step_cursor(DatalabAppState *state, int delta);
void datalab_trace_set_cursor_home(DatalabAppState *state);
void datalab_trace_set_cursor_end(DatalabAppState *state);
void datalab_trace_cycle_zoom(DatalabAppState *state);
void datalab_trace_toggle_selection(DatalabAppState *state);
void datalab_trace_request_lane_cycle(DatalabAppState *state);
void datalab_trace_clamp_cursor_to_count(DatalabAppState *state, size_t time_count);
void datalab_trace_apply_lane_cycle(DatalabAppState *state, size_t lane_count);
uint8_t datalab_workspace_authoring_theme_preset_clamp(int value);
int datalab_workspace_authoring_custom_theme_slot_clamp(int value);
int datalab_workspace_authoring_custom_theme_token_clamp(int value);
int datalab_workspace_authoring_custom_theme_channel_clamp(int value);
uint8_t datalab_workspace_authoring_cycle_runtime_theme_preset(uint8_t current, int direction);
void datalab_workspace_authoring_sync_custom_theme_from_active_slot(DatalabAppState *state);
void datalab_workspace_authoring_capture_entry_snapshot(DatalabAppState *state);
void datalab_workspace_authoring_projection_init(DatalabWorkspaceAuthoringProjection *projection);
void datalab_workspace_authoring_projection_capture_entry(DatalabAppState *state);
void datalab_workspace_authoring_projection_restore_entry(DatalabAppState *state);
int datalab_workspace_authoring_projection_apply_drag(DatalabAppState *state, float delta_x, float viewport_width);
int datalab_workspace_authoring_projection_set_profile_surface_ratio(DatalabAppState *state, float ratio);
int datalab_workspace_authoring_projection_solve(const DatalabAppState *state,
                                                 int viewport_width,
                                                 int viewport_height,
                                                 CorePaneLeafRect out_rects[2]);
void datalab_workspace_authoring_session_init(DatalabAppState *state);
void datalab_workspace_authoring_begin_takeover(DatalabAppState *state);
void datalab_workspace_authoring_cycle_overlay(DatalabAppState *state);
void datalab_workspace_authoring_apply_takeover(DatalabAppState *state);
int datalab_workspace_authoring_cancel_and_exit(DatalabAppState *state);
int datalab_workspace_authoring_runtime_mutation_allowed(const DatalabAppState *state);
void datalab_workspace_authoring_recover_failed_safe(DatalabAppState *state);
void datalab_workspace_authoring_shutdown(DatalabAppState *state);
int datalab_workspace_authoring_close_custom_theme_popup(DatalabAppState *state);
void datalab_raster_viewport_state_init(DatalabRasterViewportState *state);
void datalab_raster_viewport_request_reset(DatalabRasterViewportState *state);
void datalab_raster_viewport_sync_state(DatalabRasterViewportState *state,
                                        int view_width,
                                        int view_height,
                                        uint32_t content_width,
                                        uint32_t content_height);
int datalab_raster_viewport_zoom_at_screen_anchor(DatalabRasterViewportState *state,
                                                  int screen_x,
                                                  int screen_y,
                                                  float zoom_factor);
int datalab_raster_viewport_begin_drag(DatalabRasterViewportState *state, int screen_x, int screen_y);
void datalab_raster_viewport_end_drag(DatalabRasterViewportState *state);
int datalab_raster_viewport_drag_to(DatalabRasterViewportState *state, int screen_x, int screen_y);
void datalab_raster_viewport_copy_for_runtime(DatalabRasterViewportState *dst,
                                              const DatalabRasterViewportState *src);
int datalab_profile_supports_raster_viewport(DatalabProfile profile);
void datalab_raster_viewport_toggle_actual_pixel(DatalabAppState *state);
void datalab_raster_probe_at_screen(DatalabAppState *state, int screen_x, int screen_y);
void datalab_sampling_mode_cycle(DatalabAppState *state);

#endif
