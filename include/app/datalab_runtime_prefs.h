#ifndef DATALAB_RUNTIME_PREFS_H
#define DATALAB_RUNTIME_PREFS_H

#include <stddef.h>
#include <stdint.h>

#include "app/app_state.h"

typedef enum DatalabStartupSurface {
    DATALAB_STARTUP_SURFACE_PICKER = 0,
    DATALAB_STARTUP_SURFACE_VIEWER = 1
} DatalabStartupSurface;

int datalab_runtime_prefs_load_text_zoom_step(int *out_step);
int datalab_runtime_prefs_load_input_root(char *out_path, size_t out_cap);
int datalab_runtime_prefs_load_last_opened_input_file(char *out_path, size_t out_cap);
int datalab_runtime_prefs_load_startup_surface(DatalabStartupSurface *out_surface);
int datalab_runtime_prefs_load_recent_input_roots(char out_paths[][DATALAB_APP_PATH_CAP],
                                                  size_t path_capacity,
                                                  size_t *out_count);
int datalab_runtime_prefs_load_recent_input_files(char out_paths[][DATALAB_APP_PATH_CAP],
                                                  size_t path_capacity,
                                                  size_t *out_count);
int datalab_runtime_prefs_load_pinned_input_files(char out_paths[][DATALAB_APP_PATH_CAP],
                                                  size_t path_capacity,
                                                  size_t *out_count);
int datalab_runtime_prefs_load_theme_preset_id(uint8_t *out_theme_preset_id);
int datalab_runtime_prefs_load_workspace_authoring_profile_surface_ratio(float *out_ratio);
int datalab_runtime_prefs_load_custom_theme(DatalabWorkspaceCustomTheme *out_theme);
int datalab_runtime_prefs_load_custom_theme_slots(DatalabWorkspaceCustomTheme *out_slots, size_t slot_count);
int datalab_runtime_prefs_load_custom_theme_slot_names(char out_names[][DATALAB_CUSTOM_THEME_NAME_CAP], size_t slot_count);
int datalab_runtime_prefs_load_custom_theme_active_slot(uint8_t *out_slot);
int datalab_runtime_prefs_save_text_zoom_step(int step);
int datalab_runtime_prefs_save_input_root(const char *path);
int datalab_runtime_prefs_save_last_opened_input_file(const char *path);
int datalab_runtime_prefs_save_startup_surface(DatalabStartupSurface surface);
int datalab_runtime_prefs_save_recent_input_roots(const char paths[][DATALAB_APP_PATH_CAP], size_t count);
int datalab_runtime_prefs_save_recent_input_files(const char paths[][DATALAB_APP_PATH_CAP], size_t count);
int datalab_runtime_prefs_save_pinned_input_files(const char paths[][DATALAB_APP_PATH_CAP], size_t count);
int datalab_runtime_prefs_save_theme_preset_id(uint8_t theme_preset_id);
int datalab_runtime_prefs_save_workspace_authoring_profile_surface_ratio(float ratio);
int datalab_runtime_prefs_save_custom_theme(const DatalabWorkspaceCustomTheme *theme);
int datalab_runtime_prefs_save_custom_theme_slots(const DatalabWorkspaceCustomTheme *slots, size_t slot_count);
int datalab_runtime_prefs_save_custom_theme_slot_names(const char names[][DATALAB_CUSTOM_THEME_NAME_CAP], size_t slot_count);
int datalab_runtime_prefs_save_custom_theme_active_slot(uint8_t slot);
const char *datalab_runtime_prefs_last_diagnostic(void);
void datalab_runtime_prefs_clear_diagnostic(void);
void datalab_normalize_input_root_path(char *path, size_t path_cap);
void datalab_recent_input_roots_add(char paths[][DATALAB_APP_PATH_CAP],
                                    size_t *io_count,
                                    size_t path_capacity,
                                    const char *path);
void datalab_recent_input_files_add(char paths[][DATALAB_APP_PATH_CAP],
                                    size_t *io_count,
                                    size_t path_capacity,
                                    const char *path);
int datalab_input_root_select_recent(char *io_input_root,
                                     size_t input_root_cap,
                                     char recent_paths[][DATALAB_APP_PATH_CAP],
                                     size_t *io_recent_count,
                                     size_t recent_capacity,
                                     const char *path);

#endif
