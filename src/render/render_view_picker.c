#include "render/render_view.h"
#include "render/render_view_internal.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "kit_workspace_authoring.h"
#include "app/datalab_runtime_prefs.h"
#include "render/render_view_authoring_overlay_shared.h"

#define DATALAB_PICKER_MAX_FILES 256

typedef struct DatalabPickerRecentUiState {
    SDL_Rect button_rect;
    SDL_Rect list_rect;
    SDL_Rect item_rects[DATALAB_RECENT_INPUT_ROOT_LIMIT];
    size_t visible_count;
} DatalabPickerRecentUiState;

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

static void datalab_picker_theme_palette(DatalabWorkspaceAuthoringThemePreset theme_preset,
                                         const DatalabWorkspaceCustomTheme *custom_theme,
                                         DatalabPickerThemePalette *out_palette) {
    DatalabAuthoringThemePalette source = {0};
    if (!out_palette) {
        return;
    }
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

static int datalab_picker_zoom_modifier_active(SDL_Keymod mods) {
    return ((mods & KMOD_CTRL) != 0) || ((mods & KMOD_GUI) != 0);
}

static uint32_t datalab_picker_mod_bits_from_sdl(SDL_Keymod mods) {
    uint32_t flags = 0u;
    if ((mods & KMOD_SHIFT) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    }
    if ((mods & KMOD_ALT) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    }
    if ((mods & KMOD_CTRL) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    }
    if ((mods & KMOD_GUI) != 0) {
        flags |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    }
    return flags;
}

static KitWorkspaceAuthoringKey datalab_picker_key_from_sdl(SDL_Keycode key) {
    switch (key) {
        case SDLK_c:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDLK_v:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        default:
            return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

static int datalab_is_directory(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

static size_t datalab_picker_scan_files(const char *root,
                                        char files[][DATALAB_APP_PATH_CAP],
                                        char *status,
                                        size_t status_cap) {
    DatalabSupportedFileScanResult scan =
        datalab_scan_supported_files(root, files, DATALAB_PICKER_MAX_FILES);
    if (status && status_cap > 0u) {
        datalab_format_supported_file_scan_status(&scan,
                                                  root,
                                                  "choose folder",
                                                  status,
                                                  status_cap);
    }
    return scan.file_count;
}

static int datalab_pick_folder_macos(char *out_path, size_t out_cap) {
#if defined(__APPLE__)
    FILE *pipe = NULL;
    char line[DATALAB_APP_PATH_CAP];
    if (!out_path || out_cap == 0u) {
        return 0;
    }
    pipe = popen("/usr/bin/osascript -e 'POSIX path of (choose folder with prompt \"Choose DataLab Input Folder\")'", "r");
    if (!pipe) {
        return 0;
    }
    if (!fgets(line, sizeof(line), pipe)) {
        (void)pclose(pipe);
        return 0;
    }
    (void)pclose(pipe);
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') {
        return 0;
    }
    snprintf(out_path, out_cap, "%s", line);
    return 1;
#else
    (void)out_path;
    (void)out_cap;
    return 0;
#endif
}

CoreResult datalab_render_pick_pack_path(const char *initial_input_root,
                                         const char *initial_status,
                                         char *io_input_root,
                                         size_t input_root_cap,
                                         int *io_text_zoom_step,
                                         uint8_t *io_theme_preset_id,
                                         DatalabWorkspaceCustomTheme *io_custom_theme,
                                         int *out_enter_authoring,
                                         char *out_pack_path,
                                         size_t out_pack_path_cap) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    int done = 0;
    int canceled = 0;
    int edit_mode = 0;
    int picker_zoom_step = 0;
    DatalabWorkspaceAuthoringThemePreset picker_theme_preset_id =
        DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    DatalabWorkspaceCustomTheme picker_custom_theme = {
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
    size_t file_count = 0u;
    int selected = 0;
    char input_root[DATALAB_APP_PATH_CAP];
    char edit_root[DATALAB_APP_PATH_CAP];
    char status[256];
    char files[DATALAB_PICKER_MAX_FILES][DATALAB_APP_PATH_CAP];
    char recent_input_roots[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP];
    size_t recent_input_root_count = 0u;
    int recent_input_root_dropdown_open = 0;
    DatalabPickerRecentUiState recent_ui = {0};

    if (!io_input_root || input_root_cap == 0u || !out_pack_path || out_pack_path_cap == 0u) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid picker arguments" };
    }
    if (io_text_zoom_step) {
        picker_zoom_step = datalab_text_zoom_step_clamp(*io_text_zoom_step);
        datalab_set_text_zoom_step(picker_zoom_step);
    }
    if (io_theme_preset_id) {
        picker_theme_preset_id = datalab_overlay_theme_preset_clamp((int)(*io_theme_preset_id));
    }
    if (io_custom_theme) {
        picker_custom_theme = *io_custom_theme;
    }
    out_pack_path[0] = '\0';
    if (out_enter_authoring) {
        *out_enter_authoring = 0;
    }
    if (initial_input_root && initial_input_root[0] != '\0') {
        snprintf(input_root, sizeof(input_root), "%s", initial_input_root);
    } else if (io_input_root[0] != '\0') {
        snprintf(input_root, sizeof(input_root), "%s", io_input_root);
    } else {
        snprintf(input_root, sizeof(input_root), "%s", "data/import");
    }
    snprintf(edit_root, sizeof(edit_root), "%s", input_root);
    (void)datalab_runtime_prefs_load_recent_input_roots(recent_input_roots,
                                                        DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                        &recent_input_root_count);
    (void)datalab_input_root_select_recent(input_root,
                                           sizeof(input_root),
                                           recent_input_roots,
                                           &recent_input_root_count,
                                           DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                           input_root);
    status[0] = '\0';
    if (initial_status && initial_status[0] != '\0') {
        snprintf(status, sizeof(status), "%s", initial_status);
    }

    file_count = datalab_picker_scan_files(input_root,
                                           files,
                                           status[0] ? NULL : status,
                                           status[0] ? 0u : sizeof(status));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    window = SDL_CreateWindow("DataLab | Select Input Root + Pack",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              1160,
                              820,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        SDL_Quit();
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }

    SDL_StartTextInput();
    while (!done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                canceled = 1;
                done = 1;
                break;
            }
            if (e.type == SDL_TEXTINPUT && edit_mode) {
                size_t cur = strlen(edit_root);
                size_t add = strlen(e.text.text);
                if (cur + add + 1u < sizeof(edit_root)) {
                    strncat(edit_root, e.text.text, sizeof(edit_root) - cur - 1u);
                }
                continue;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int pointer_x = 0;
                int pointer_y = 0;
                size_t recent_idx = 0u;
                if (!datalab_render_map_window_to_renderer_point(window,
                                                                 renderer,
                                                                 e.button.x,
                                                                 e.button.y,
                                                                 &pointer_x,
                                                                 &pointer_y)) {
                    continue;
                }
                if (datalab_render_point_in_rect(&recent_ui.button_rect, pointer_x, pointer_y)) {
                    recent_input_root_dropdown_open = !recent_input_root_dropdown_open;
                    continue;
                }
                if (recent_input_root_dropdown_open) {
                    int handled_recent = 0;
                    for (recent_idx = 0u; recent_idx < recent_ui.visible_count; ++recent_idx) {
                        if (!datalab_render_point_in_rect(&recent_ui.item_rects[recent_idx], pointer_x, pointer_y)) {
                            continue;
                        }
                        if (recent_idx < recent_input_root_count && recent_input_roots[recent_idx][0] != '\0') {
                            (void)datalab_input_root_select_recent(input_root,
                                                                   sizeof(input_root),
                                                                   recent_input_roots,
                                                                   &recent_input_root_count,
                                                                   DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                                   recent_input_roots[recent_idx]);
                            snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                            file_count = datalab_picker_scan_files(input_root, files, status, sizeof(status));
                            selected = 0;
                            edit_mode = 0;
                            recent_input_root_dropdown_open = 0;
                            handled_recent = 1;
                            break;
                        }
                    }
                    if (handled_recent) {
                        continue;
                    }
                    if (!datalab_render_point_in_rect(&recent_ui.list_rect, pointer_x, pointer_y)) {
                        recent_input_root_dropdown_open = 0;
                    }
                    continue;
                }
            }
            if (e.type != SDL_KEYDOWN) {
                continue;
            }
            {
                const uint8_t *keyboard = SDL_GetKeyboardState(NULL);
                const KitWorkspaceAuthoringKey authoring_key =
                    datalab_picker_key_from_sdl(e.key.keysym.sym);
                const uint32_t mod_bits =
                    datalab_picker_mod_bits_from_sdl((SDL_Keymod)e.key.keysym.mod);
                const int key_c_down = (keyboard && keyboard[SDL_SCANCODE_C] != 0) ? 1 : 0;
                const int key_v_down = (keyboard && keyboard[SDL_SCANCODE_V] != 0) ? 1 : 0;

                if ((mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) != 0u &&
                    (authoring_key == KIT_WORKSPACE_AUTHORING_KEY_C ||
                     authoring_key == KIT_WORKSPACE_AUTHORING_KEY_V)) {
                    if (kit_workspace_authoring_entry_chord_pressed(authoring_key,
                                                                    mod_bits,
                                                                    key_c_down,
                                                                    key_v_down)) {
                        if (file_count > 0u && selected >= 0 && selected < (int)file_count) {
                            if (datalab_input_root_join_child_file(input_root,
                                                                   files[selected],
                                                                   out_pack_path,
                                                                   out_pack_path_cap)) {
                                if (out_enter_authoring) {
                                    *out_enter_authoring = 1;
                                }
                                done = 1;
                            } else {
                                snprintf(status, sizeof(status), "selected file is outside input root");
                            }
                        } else {
                            snprintf(status, sizeof(status), "authoring entry requires a selected file");
                        }
                    }
                    continue;
                }
            }
            if (datalab_picker_zoom_modifier_active((SDL_Keymod)e.key.keysym.mod)) {
                switch (e.key.keysym.sym) {
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                    case SDLK_KP_PLUS:
                        picker_zoom_step = datalab_text_zoom_step_clamp(picker_zoom_step + 1);
                        datalab_set_text_zoom_step(picker_zoom_step);
                        continue;
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                        picker_zoom_step = datalab_text_zoom_step_clamp(picker_zoom_step - 1);
                        datalab_set_text_zoom_step(picker_zoom_step);
                        continue;
                    case SDLK_0:
                    case SDLK_KP_0:
                        picker_zoom_step = 0;
                        datalab_set_text_zoom_step(picker_zoom_step);
                        continue;
                    default:
                        break;
                }
            }
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    if (edit_mode) {
                        edit_mode = 0;
                        snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                    } else if (recent_input_root_dropdown_open) {
                        recent_input_root_dropdown_open = 0;
                    } else {
                        canceled = 1;
                        done = 1;
                    }
                    break;
                case SDLK_e:
                    recent_input_root_dropdown_open = 0;
                    edit_mode = !edit_mode;
                    if (edit_mode) {
                        snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                    }
                    break;
                case SDLK_b:
                    if (!edit_mode) {
                        char picked[DATALAB_APP_PATH_CAP];
                        if (datalab_pick_folder_macos(picked, sizeof(picked))) {
                            (void)datalab_input_root_select_recent(input_root,
                                                                   sizeof(input_root),
                                                                   recent_input_roots,
                                                                   &recent_input_root_count,
                                                                   DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                                   picked);
                            snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                            file_count = datalab_picker_scan_files(input_root, files, status, sizeof(status));
                            selected = 0;
                        } else {
                            snprintf(status, sizeof(status), "folder dialog canceled/unavailable");
                        }
                    }
                    break;
                case SDLK_BACKSPACE:
                    if (edit_mode) {
                        size_t len = strlen(edit_root);
                        if (len > 0u) {
                            edit_root[len - 1u] = '\0';
                        }
                    }
                    break;
                case SDLK_UP:
                    if (!edit_mode && selected > 0) {
                        selected--;
                    }
                    break;
                case SDLK_DOWN:
                    if (!edit_mode && selected + 1 < (int)file_count) {
                        selected++;
                    }
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (edit_mode) {
                        if (!datalab_is_directory(edit_root)) {
                            snprintf(status, sizeof(status), "invalid directory: %s", edit_root);
                            break;
                        }
                        (void)datalab_input_root_select_recent(input_root,
                                                               sizeof(input_root),
                                                               recent_input_roots,
                                                               &recent_input_root_count,
                                                               DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                               edit_root);
                        file_count = datalab_picker_scan_files(input_root, files, status, sizeof(status));
                        selected = 0;
                        edit_mode = 0;
                        break;
                    }
                    if (file_count > 0u && selected >= 0 && selected < (int)file_count) {
                        if (datalab_input_root_join_child_file(input_root,
                                                               files[selected],
                                                               out_pack_path,
                                                               out_pack_path_cap)) {
                            done = 1;
                        } else {
                            snprintf(status, sizeof(status), "selected file is outside input root");
                        }
                    } else {
                        snprintf(status, sizeof(status), "no file selected");
                    }
                    break;
                default:
                    break;
            }
        }

        {
            int ww = 0;
            int wh = 0;
            DatalabPickerThemePalette palette = {0};
            SDL_Rect top = {0};
            SDL_Rect list = {0};
            SDL_Rect top_clip = {0};
            SDL_Rect list_clip = {0};
            SDL_Rect recent_button = {0};
            SDL_Rect recent_list = {0};
            SDL_Rect recent_clip = {0};
            int pad = 0;
            int line_h1 = 0;
            int line_h2 = 0;
            int row_h = 0;
            int recent_row_h = 0;
            int top_gap = 0;
            int list_gap = 0;
            int recent_visible = 0;
            int y_title = 0;
            int y_help = 0;
            int y_path_label = 0;
            int y_path_value = 0;
            int y_status = 0;
            int list_start_y = 0;
            int visible_lines = 0;
            int start_idx = 0;
            SDL_GetRendererOutputSize(renderer, &ww, &wh);
            datalab_picker_theme_palette(picker_theme_preset_id, &picker_custom_theme, &palette);
            line_h1 = datalab_text_line_height(1);
            line_h2 = datalab_text_line_height(2);
            pad = datalab_scaled_px(10.0f);
            top_gap = datalab_scaled_px(6.0f);
            list_gap = datalab_scaled_px(4.0f);
            row_h = line_h1 + datalab_scaled_px(4.0f);
            recent_row_h = line_h1 + datalab_scaled_px(4.0f);
            if (row_h < datalab_scaled_px(10.0f)) {
                row_h = datalab_scaled_px(10.0f);
            }
            if (recent_row_h < datalab_scaled_px(10.0f)) {
                recent_row_h = datalab_scaled_px(10.0f);
            }
            recent_visible = (int)recent_input_root_count;
            if (recent_visible > DATALAB_RECENT_INPUT_ROOT_LIMIT) {
                recent_visible = DATALAB_RECENT_INPUT_ROOT_LIMIT;
            }

            top.x = datalab_scaled_px(18.0f);
            top.y = datalab_scaled_px(18.0f);
            top.w = ww - datalab_scaled_px(36.0f);

            y_title = top.y + pad;
            y_help = y_title + line_h2 + top_gap;
            y_path_label = y_help + line_h1 + top_gap;
            y_path_value = y_path_label + line_h1 + top_gap;
            y_status = y_path_value + line_h1 + top_gap;
            top.h = (y_status - top.y) + line_h1 + pad;
            if (top.h < datalab_scaled_px(130.0f)) {
                top.h = datalab_scaled_px(130.0f);
            }

            list.x = datalab_scaled_px(18.0f);
            list.y = top.y + top.h + datalab_scaled_px(12.0f);
            list.w = ww - datalab_scaled_px(36.0f);
            list.h = wh - list.y - datalab_scaled_px(18.0f);
            top_clip.x = top.x + pad;
            top_clip.y = top.y + datalab_scaled_px(6.0f);
            top_clip.w = top.w - (pad * 2);
            top_clip.h = top.h - datalab_scaled_px(12.0f);
            list_clip.x = list.x + datalab_scaled_px(6.0f);
            list_clip.y = list.y + datalab_scaled_px(4.0f);
            list_clip.w = list.w - datalab_scaled_px(12.0f);
            list_clip.h = list.h - datalab_scaled_px(8.0f);
            list_start_y = list.y + list_gap;

            SDL_SetRenderDrawColor(renderer, palette.clear_r, palette.clear_g, palette.clear_b, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, palette.top_fill_r, palette.top_fill_g, palette.top_fill_b, 255);
            SDL_RenderFillRect(renderer, &top);
            SDL_SetRenderDrawColor(renderer, palette.frame_r, palette.frame_g, palette.frame_b, 255);
            SDL_RenderDrawRect(renderer, &top);
            SDL_SetRenderDrawColor(renderer, palette.list_fill_r, palette.list_fill_g, palette.list_fill_b, 255);
            SDL_RenderFillRect(renderer, &list);
            SDL_SetRenderDrawColor(renderer, palette.frame_r, palette.frame_g, palette.frame_b, 255);
            SDL_RenderDrawRect(renderer, &list);

            recent_button = (SDL_Rect){
                top.x + top.w - datalab_scaled_px(320.0f) - pad,
                top.y + pad,
                datalab_scaled_px(320.0f),
                (line_h1 * 2) + datalab_scaled_px(8.0f)
            };
            if (recent_button.x < top.x + datalab_scaled_px(320.0f)) {
                recent_button.x = top.x + datalab_scaled_px(320.0f);
                recent_button.w = top.x + top.w - recent_button.x - pad;
            }
            SDL_SetRenderDrawColor(renderer,
                                   palette.selected_fill_r,
                                   palette.selected_fill_g,
                                   palette.selected_fill_b,
                                   180);
            SDL_RenderFillRect(renderer, &recent_button);
            SDL_SetRenderDrawColor(renderer,
                                   palette.selected_border_r,
                                   palette.selected_border_g,
                                   palette.selected_border_b,
                                   255);
            SDL_RenderDrawRect(renderer, &recent_button);
            draw_text_5x7(renderer,
                          recent_button.x + datalab_scaled_px(8.0f),
                          recent_button.y + datalab_scaled_px(4.0f),
                          "RECENT DIRECTORIES",
                          1,
                          palette.text_primary_r,
                          palette.text_primary_g,
                          palette.text_primary_b,
                          255);
            recent_clip = (SDL_Rect){
                recent_button.x + datalab_scaled_px(8.0f),
                recent_button.y + line_h1 + datalab_scaled_px(2.0f),
                recent_button.w - datalab_scaled_px(16.0f),
                line_h1
            };
            draw_text_5x7_clipped(renderer,
                                  &recent_clip,
                                  recent_clip.x,
                                  recent_clip.y,
                                  input_root,
                                  1,
                                  palette.text_secondary_r,
                                  palette.text_secondary_g,
                                  palette.text_secondary_b,
                                  255);
            recent_ui.button_rect = recent_button;
            recent_ui.list_rect = (SDL_Rect){0, 0, 0, 0};
            recent_ui.visible_count = 0u;
            if (recent_input_root_dropdown_open && recent_visible > 0) {
                recent_list = (SDL_Rect){
                    recent_button.x,
                    recent_button.y + recent_button.h + datalab_scaled_px(4.0f),
                    recent_button.w,
                    (recent_visible * recent_row_h) + (pad * 2)
                };
                SDL_SetRenderDrawColor(renderer, palette.top_fill_r, palette.top_fill_g, palette.top_fill_b, 248);
                SDL_RenderFillRect(renderer, &recent_list);
                SDL_SetRenderDrawColor(renderer, palette.frame_r, palette.frame_g, palette.frame_b, 255);
                SDL_RenderDrawRect(renderer, &recent_list);
                recent_clip = (SDL_Rect){
                    recent_list.x + pad,
                    recent_list.y + pad,
                    recent_list.w - (pad * 2),
                    recent_list.h - (pad * 2)
                };
                recent_ui.list_rect = recent_list;
                recent_ui.visible_count = (size_t)recent_visible;
                for (int recent_i = 0; recent_i < recent_visible; ++recent_i) {
                    SDL_Rect item_rect = {
                        recent_list.x + pad,
                        recent_list.y + pad + (recent_i * recent_row_h),
                        recent_list.w - (pad * 2),
                        recent_row_h
                    };
                    int is_active_recent = strcmp(recent_input_roots[recent_i], input_root) == 0;
                    if (is_active_recent) {
                        SDL_SetRenderDrawColor(renderer,
                                               palette.selected_fill_r,
                                               palette.selected_fill_g,
                                               palette.selected_fill_b,
                                               200);
                        SDL_RenderFillRect(renderer, &item_rect);
                        SDL_SetRenderDrawColor(renderer,
                                               palette.selected_border_r,
                                               palette.selected_border_g,
                                               palette.selected_border_b,
                                               255);
                        SDL_RenderDrawRect(renderer, &item_rect);
                    }
                    draw_text_5x7_clipped(renderer,
                                          &recent_clip,
                                          item_rect.x + datalab_scaled_px(4.0f),
                                          item_rect.y + datalab_scaled_px(2.0f),
                                          recent_input_roots[recent_i],
                                          1,
                                          is_active_recent ? palette.text_primary_r : palette.text_secondary_r,
                                          is_active_recent ? palette.text_primary_g : palette.text_secondary_g,
                                          is_active_recent ? palette.text_primary_b : palette.text_secondary_b,
                                          255);
                    recent_ui.item_rects[recent_i] = item_rect;
                }
            }

            draw_text_5x7(renderer, top.x + pad, y_title,
                          "DATALAB INPUT ROOT + DATA PICKER", 2,
                          palette.text_primary_r, palette.text_primary_g, palette.text_primary_b, 255);
            draw_text_5x7(renderer, top.x + pad, y_help,
                          "ALT+C+V OPEN+AUTHOR  E EDIT PATH  ENTER APPLY  B FOLDER DIALOG  CLICK RECENT DIRS  UP/DOWN SELECT  ENTER OPEN FILE  ESC CANCEL",
                          1, palette.text_secondary_r, palette.text_secondary_g, palette.text_secondary_b, 255);
            draw_text_5x7(renderer, top.x + pad, y_path_label,
                          edit_mode ? "PATH (EDIT MODE):" : "PATH:",
                          1, palette.text_muted_r, palette.text_muted_g, palette.text_muted_b, 255);
            draw_text_5x7_clipped(renderer,
                                  &top_clip,
                                  top.x + pad,
                                  y_path_value,
                                  edit_mode ? edit_root : input_root,
                                  1,
                                  palette.text_primary_r,
                                  palette.text_primary_g,
                                  palette.text_primary_b,
                                  255);
            draw_text_5x7(renderer, top.x + pad, y_status,
                          "STATUS:", 1, palette.text_muted_r, palette.text_muted_g, palette.text_muted_b, 255);
            draw_text_5x7_clipped(renderer,
                                  &top_clip,
                                  top.x + pad + datalab_scaled_px(58.0f),
                                  y_status,
                                  status,
                                  1,
                                  palette.text_success_r,
                                  palette.text_success_g,
                                  palette.text_success_b,
                                  255);

            visible_lines = (list.h - (list_gap * 2)) / row_h;
            if (visible_lines < 1) {
                visible_lines = 1;
            }
            if (selected >= visible_lines) {
                start_idx = selected - visible_lines + 1;
            }
            if (start_idx + visible_lines > (int)file_count) {
                if ((int)file_count > visible_lines) {
                    start_idx = (int)file_count - visible_lines;
                } else {
                    start_idx = 0;
                }
            }
            for (int i = 0; i < visible_lines; ++i) {
                int idx = start_idx + i;
                int row_y = list_start_y + i * row_h;
                int text_y = row_y + ((row_h - line_h1) / 2);
                if (idx >= (int)file_count) {
                    break;
                }
                if (idx == selected) {
                    SDL_Rect hi = { list_clip.x, row_y, list_clip.w, row_h };
                    SDL_SetRenderDrawColor(renderer,
                                           palette.selected_fill_r,
                                           palette.selected_fill_g,
                                           palette.selected_fill_b,
                                           190);
                    SDL_RenderFillRect(renderer, &hi);
                    SDL_SetRenderDrawColor(renderer,
                                           palette.selected_border_r,
                                           palette.selected_border_g,
                                           palette.selected_border_b,
                                           255);
                    SDL_RenderDrawRect(renderer, &hi);
                }
                draw_text_5x7_clipped(renderer,
                                      &list_clip,
                                      list_clip.x + datalab_scaled_px(6.0f),
                                      text_y,
                                      files[idx],
                                      1,
                                      palette.text_primary_r,
                                      palette.text_primary_g,
                                      palette.text_primary_b,
                                      255);
            }

            if (file_count == 0u) {
                draw_text_5x7_clipped(renderer,
                                      &list_clip,
                                      list_clip.x + datalab_scaled_px(6.0f),
                                      list_start_y + ((row_h - line_h1) / 2),
                                      "NO SUPPORTED FILES (.PACK/.BMP) FOUND IN INPUT ROOT",
                                      1,
                                      palette.text_empty_r,
                                      palette.text_empty_g,
                                      palette.text_empty_b,
                                      255);
            }
            SDL_RenderPresent(renderer);
        }
    }

    SDL_StopTextInput();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    snprintf(io_input_root, input_root_cap, "%s", input_root);
    if (io_text_zoom_step) {
        *io_text_zoom_step = picker_zoom_step;
    }
    if (io_theme_preset_id) {
        *io_theme_preset_id = (uint8_t)picker_theme_preset_id;
    }
    if (io_custom_theme) {
        *io_custom_theme = picker_custom_theme;
    }
    datalab_runtime_prefs_save_recent_input_roots(recent_input_roots, recent_input_root_count);
    if (canceled) {
        out_pack_path[0] = '\0';
    }
    return core_result_ok();
}
