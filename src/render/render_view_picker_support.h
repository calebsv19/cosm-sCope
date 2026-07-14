#ifndef DATALAB_RENDER_VIEW_PICKER_SUPPORT_H
#define DATALAB_RENDER_VIEW_PICKER_SUPPORT_H

#include <stddef.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "app/app_state.h"
#include "kit_workspace_authoring.h"
#include "data/pack_inspector.h"

typedef struct DatalabPickerThemePalette {
    uint8_t clear_r, clear_g, clear_b;
    uint8_t top_fill_r, top_fill_g, top_fill_b;
    uint8_t list_fill_r, list_fill_g, list_fill_b;
    uint8_t frame_r, frame_g, frame_b;
    uint8_t text_primary_r, text_primary_g, text_primary_b;
    uint8_t text_secondary_r, text_secondary_g, text_secondary_b;
    uint8_t text_muted_r, text_muted_g, text_muted_b;
    uint8_t text_success_r, text_success_g, text_success_b;
    uint8_t text_empty_r, text_empty_g, text_empty_b;
    uint8_t selected_fill_r, selected_fill_g, selected_fill_b;
    uint8_t selected_border_r, selected_border_g, selected_border_b;
} DatalabPickerThemePalette;

void datalab_picker_theme_palette(DatalabWorkspaceAuthoringThemePreset theme_preset,
                                  const DatalabWorkspaceCustomTheme *custom_theme,
                                  DatalabPickerThemePalette *out_palette);
int datalab_picker_zoom_modifier_active(SDL_Keymod mods);
uint32_t datalab_picker_mod_bits_from_sdl(SDL_Keymod mods);
KitWorkspaceAuthoringKey datalab_picker_key_from_sdl(SDL_Keycode key);
int datalab_picker_is_directory(const char *path);
int datalab_picker_is_regular_file(const char *path);
const char *datalab_picker_display_path(const char *path, char *out, size_t out_cap);
int datalab_picker_parent_directory(const char *path, char *out_root, size_t out_root_cap);
void datalab_picker_draw_pack_inspection(SDL_Renderer *renderer,
                                         const SDL_Rect *rect,
                                         const DatalabPickerThemePalette *palette,
                                         const DatalabPackInspection *inspection,
                                         int inspection_valid);
size_t datalab_picker_scan_files(const char *root,
                                 char files[][DATALAB_APP_PATH_CAP],
                                 char *status,
                                 size_t status_cap);
size_t datalab_picker_apply_filter(char files[][DATALAB_APP_PATH_CAP],
                                   const char all_files[][DATALAB_APP_PATH_CAP],
                                   size_t all_count,
                                   const char *filter);
int datalab_picker_path_is_pinned(const char paths[][DATALAB_APP_PATH_CAP], size_t count, const char *path);

#endif
