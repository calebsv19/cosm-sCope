#include "render_view_picker_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "app/datalab_runtime_prefs.h"
#include "data/input_file_loader.h"
#include "render_view_authoring_overlay_shared.h"
#include "render_view_internal.h"

void datalab_picker_theme_palette(DatalabWorkspaceAuthoringThemePreset theme_preset,
                                  const DatalabWorkspaceCustomTheme *custom_theme,
                                  DatalabPickerThemePalette *out_palette) {
    DatalabAuthoringThemePalette source = {0};
    if (!out_palette) return;
    datalab_overlay_theme_palette(theme_preset, custom_theme, &source);
    *out_palette = (DatalabPickerThemePalette){
        source.clear_r, source.clear_g, source.clear_b,
        source.shell_fill_r, source.shell_fill_g, source.shell_fill_b,
        source.pane_fill_r, source.pane_fill_g, source.pane_fill_b,
        source.shell_border_r, source.shell_border_g, source.shell_border_b,
        source.text_primary_r, source.text_primary_g, source.text_primary_b,
        source.text_secondary_r, source.text_secondary_g, source.text_secondary_b,
        source.text_secondary_r, source.text_secondary_g, source.text_secondary_b,
        source.text_primary_r, source.text_primary_g, source.text_primary_b,
        source.button_fill_r, source.button_fill_g, source.button_fill_b,
        source.button_hover_r, source.button_hover_g, source.button_hover_b,
        source.button_active_r, source.button_active_g, source.button_active_b
    };
}

int datalab_picker_zoom_modifier_active(SDL_Keymod mods) {
    return ((mods & KMOD_CTRL) != 0) || ((mods & KMOD_GUI) != 0);
}

uint32_t datalab_picker_mod_bits_from_sdl(SDL_Keymod mods) {
    uint32_t flags = 0u;
    if ((mods & KMOD_SHIFT) != 0) flags |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    if ((mods & KMOD_ALT) != 0) flags |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    if ((mods & KMOD_CTRL) != 0) flags |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    if ((mods & KMOD_GUI) != 0) flags |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    return flags;
}

KitWorkspaceAuthoringKey datalab_picker_key_from_sdl(SDL_Keycode key) {
    switch (key) {
        case SDLK_c: return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDLK_v: return KIT_WORKSPACE_AUTHORING_KEY_V;
        default: return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

int datalab_picker_is_directory(const char *path) {
    struct stat st;
    return path && path[0] != '\0' && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int datalab_picker_is_regular_file(const char *path) {
    struct stat st;
    return path && path[0] != '\0' && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

const char *datalab_picker_display_path(const char *path, char *out, size_t out_cap) {
    const char *home = getenv("HOME");
    size_t home_len = home ? strlen(home) : 0u;
    if (!path || !out || out_cap == 0u) return path ? path : "";
    if (home_len > 0u && strncmp(path, home, home_len) == 0 && (path[home_len] == '/' || path[home_len] == '\0')) {
        snprintf(out, out_cap, "~%s", path + home_len);
    } else {
        snprintf(out, out_cap, "%s", path);
    }
    return out;
}

int datalab_picker_parent_directory(const char *path, char *out_root, size_t out_root_cap) {
    const char *slash = NULL;
    size_t root_len = 0u;
    if (!path || !path[0] || !out_root || out_root_cap == 0u) return 0;
    slash = strrchr(path, '/');
    if (!slash || slash == path) return 0;
    root_len = (size_t)(slash - path);
    if (root_len + 1u > out_root_cap) return 0;
    memcpy(out_root, path, root_len);
    out_root[root_len] = '\0';
    return datalab_picker_is_directory(out_root);
}

void datalab_picker_draw_pack_inspection(SDL_Renderer *renderer,
                                         const SDL_Rect *rect,
                                         const DatalabPickerThemePalette *palette,
                                         const DatalabPackInspection *inspection,
                                         int inspection_valid) {
    char line[160];
    int line_h = 0;
    int y = 0;
    if (!renderer || !rect || !palette) return;
    SDL_SetRenderDrawColor(renderer, palette->top_fill_r, palette->top_fill_g, palette->top_fill_b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, palette->frame_r, palette->frame_g, palette->frame_b, 255);
    SDL_RenderDrawRect(renderer, rect);
    line_h = datalab_text_line_height(1) + datalab_scaled_px(5.0f);
    y = rect->y + datalab_scaled_px(8.0f);
    draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), y, "PACK INSPECTOR", 1,
                  palette->text_primary_r, palette->text_primary_g, palette->text_primary_b, 255);
    y += line_h * 2;
    if (!inspection_valid || !inspection) {
        draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), y, "PACK INDEX UNAVAILABLE", 1,
                      palette->text_empty_r, palette->text_empty_g, palette->text_empty_b, 255);
        return;
    }
    datalab_pack_inspection_format_summary(inspection, line, sizeof(line));
    draw_text_5x7_clipped(renderer, rect, rect->x + datalab_scaled_px(8.0f), y, line, 1,
                           palette->text_secondary_r, palette->text_secondary_g, palette->text_secondary_b, 255);
    y += line_h * 2;
    for (size_t i = 0u; i < inspection->listed_chunk_count && y < rect->y + rect->h - line_h; ++i) {
        datalab_pack_inspection_format_chunk(inspection, i, line, sizeof(line));
        draw_text_5x7_clipped(renderer, rect, rect->x + datalab_scaled_px(8.0f), y, line, 1,
                               palette->text_primary_r, palette->text_primary_g, palette->text_primary_b, 255);
        y += line_h;
    }
    if (inspection->listed_chunk_count < inspection->chunk_count && y < rect->y + rect->h - line_h) {
        snprintf(line, sizeof(line), "+ %zu MORE CHUNKS", inspection->chunk_count - inspection->listed_chunk_count);
        draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), y, line, 1,
                      palette->text_muted_r, palette->text_muted_g, palette->text_muted_b, 255);
    }
}

size_t datalab_picker_scan_files(const char *root,
                                 char files[][DATALAB_APP_PATH_CAP],
                                 char *status,
                                 size_t status_cap) {
    DatalabSupportedFileScanResult scan = datalab_scan_supported_files(root, files, 256u);
    if (status && status_cap > 0u) datalab_format_supported_file_scan_status(&scan, root, "choose folder", status, status_cap);
    return scan.file_count;
}

static int datalab_picker_filter_matches(const char *text, const char *filter) {
    size_t start = 0u;
    size_t filter_len = 0u;
    if (!filter || filter[0] == '\0') return 1;
    if (!text) return 0;
    filter_len = strlen(filter);
    for (; text[start] != '\0'; ++start) {
        size_t i = 0u;
        while (i < filter_len && text[start + i] != '\0') {
            unsigned char a = (unsigned char)text[start + i];
            unsigned char b = (unsigned char)filter[i];
            if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
            if (a != b) break;
            ++i;
        }
        if (i == filter_len) return 1;
    }
    return 0;
}

size_t datalab_picker_apply_filter(char files[][DATALAB_APP_PATH_CAP],
                                   const char all_files[][DATALAB_APP_PATH_CAP],
                                   size_t all_count,
                                   const char *filter) {
    size_t count = 0u;
    for (size_t i = 0u; i < all_count; ++i) {
        if (datalab_picker_filter_matches(all_files[i], filter)) {
            snprintf(files[count++], DATALAB_APP_PATH_CAP, "%s", all_files[i]);
        }
    }
    return count;
}

int datalab_picker_path_is_pinned(const char paths[][DATALAB_APP_PATH_CAP], size_t count, const char *path) {
    for (size_t i = 0u; i < count; ++i) if (strcmp(paths[i], path) == 0) return 1;
    return 0;
}
