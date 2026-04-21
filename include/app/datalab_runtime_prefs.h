#ifndef DATALAB_RUNTIME_PREFS_H
#define DATALAB_RUNTIME_PREFS_H

#include <stddef.h>
#include <stdint.h>

#include "app/app_state.h"

int datalab_runtime_prefs_load_text_zoom_step(int *out_step);
int datalab_runtime_prefs_load_input_root(char *out_path, size_t out_cap);
int datalab_runtime_prefs_load_theme_preset_id(uint8_t *out_theme_preset_id);
int datalab_runtime_prefs_load_custom_theme(DatalabWorkspaceCustomTheme *out_theme);
int datalab_runtime_prefs_load_custom_theme_slots(DatalabWorkspaceCustomTheme *out_slots, size_t slot_count);
int datalab_runtime_prefs_load_custom_theme_slot_names(char out_names[][DATALAB_CUSTOM_THEME_NAME_CAP], size_t slot_count);
int datalab_runtime_prefs_load_custom_theme_active_slot(uint8_t *out_slot);
void datalab_runtime_prefs_save_text_zoom_step(int step);
void datalab_runtime_prefs_save_input_root(const char *path);
void datalab_runtime_prefs_save_theme_preset_id(uint8_t theme_preset_id);
void datalab_runtime_prefs_save_custom_theme(const DatalabWorkspaceCustomTheme *theme);
void datalab_runtime_prefs_save_custom_theme_slots(const DatalabWorkspaceCustomTheme *slots, size_t slot_count);
void datalab_runtime_prefs_save_custom_theme_slot_names(const char names[][DATALAB_CUSTOM_THEME_NAME_CAP], size_t slot_count);
void datalab_runtime_prefs_save_custom_theme_active_slot(uint8_t slot);

#endif
