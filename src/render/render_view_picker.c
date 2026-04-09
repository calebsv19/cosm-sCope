#include "render/render_view.h"
#include "render/render_view_internal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define DATALAB_PICKER_MAX_FILES 256

static int datalab_picker_zoom_modifier_active(SDL_Keymod mods) {
    return ((mods & KMOD_CTRL) != 0) || ((mods & KMOD_GUI) != 0);
}

static int datalab_has_pack_extension(const char *name) {
    size_t len = 0u;
    if (!name) {
        return 0;
    }
    len = strlen(name);
    if (len < 5u) {
        return 0;
    }
    return strcasecmp(name + len - 5u, ".pack") == 0;
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

static int datalab_cmp_names(const void *a, const void *b) {
    const char *aa = (const char *)a;
    const char *bb = (const char *)b;
    return strcasecmp(aa, bb);
}

static size_t datalab_scan_pack_files(const char *root,
                                      char files[][DATALAB_APP_PATH_CAP],
                                      size_t max_files,
                                      char *status,
                                      size_t status_cap) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    size_t count = 0u;
    if (!root || !files || max_files == 0u) {
        if (status && status_cap > 0u) {
            snprintf(status, status_cap, "invalid scan request");
        }
        return 0u;
    }
    dir = opendir(root);
    if (!dir) {
        if (status && status_cap > 0u) {
            snprintf(status, status_cap, "cannot open: %s", root);
        }
        return 0u;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!datalab_has_pack_extension(entry->d_name)) {
            continue;
        }
        if (count >= max_files) {
            break;
        }
        snprintf(files[count], DATALAB_APP_PATH_CAP, "%s", entry->d_name);
        count++;
    }
    closedir(dir);
    if (count > 1u) {
        qsort(files, count, sizeof(files[0]), datalab_cmp_names);
    }
    if (status && status_cap > 0u) {
        snprintf(status, status_cap, "found %zu .pack files", count);
    }
    return count;
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
                                         char *io_input_root,
                                         size_t input_root_cap,
                                         int *io_text_zoom_step,
                                         char *out_pack_path,
                                         size_t out_pack_path_cap) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    int done = 0;
    int canceled = 0;
    int edit_mode = 0;
    int picker_zoom_step = 0;
    size_t file_count = 0u;
    int selected = 0;
    char input_root[DATALAB_APP_PATH_CAP];
    char edit_root[DATALAB_APP_PATH_CAP];
    char status[256];
    char files[DATALAB_PICKER_MAX_FILES][DATALAB_APP_PATH_CAP];

    if (!io_input_root || input_root_cap == 0u || !out_pack_path || out_pack_path_cap == 0u) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid picker arguments" };
    }
    if (io_text_zoom_step) {
        picker_zoom_step = datalab_text_zoom_step_clamp(*io_text_zoom_step);
        datalab_set_text_zoom_step(picker_zoom_step);
    }
    out_pack_path[0] = '\0';
    if (initial_input_root && initial_input_root[0] != '\0') {
        snprintf(input_root, sizeof(input_root), "%s", initial_input_root);
    } else if (io_input_root[0] != '\0') {
        snprintf(input_root, sizeof(input_root), "%s", io_input_root);
    } else {
        snprintf(input_root, sizeof(input_root), "%s", "data/import");
    }
    snprintf(edit_root, sizeof(edit_root), "%s", input_root);
    status[0] = '\0';

    file_count = datalab_scan_pack_files(input_root, files, DATALAB_PICKER_MAX_FILES, status, sizeof(status));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    window = SDL_CreateWindow("DataLab | Select Input Root + Pack",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              1160,
                              820,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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
            if (e.type != SDL_KEYDOWN) {
                continue;
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
                    } else {
                        canceled = 1;
                        done = 1;
                    }
                    break;
                case SDLK_e:
                    edit_mode = !edit_mode;
                    if (edit_mode) {
                        snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                    }
                    break;
                case SDLK_b:
                    if (!edit_mode) {
                        char picked[DATALAB_APP_PATH_CAP];
                        if (datalab_pick_folder_macos(picked, sizeof(picked))) {
                            snprintf(input_root, sizeof(input_root), "%s", picked);
                            snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                            file_count = datalab_scan_pack_files(input_root,
                                                                 files,
                                                                 DATALAB_PICKER_MAX_FILES,
                                                                 status,
                                                                 sizeof(status));
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
                        snprintf(input_root, sizeof(input_root), "%s", edit_root);
                        file_count = datalab_scan_pack_files(input_root,
                                                             files,
                                                             DATALAB_PICKER_MAX_FILES,
                                                             status,
                                                             sizeof(status));
                        selected = 0;
                        edit_mode = 0;
                        break;
                    }
                    if (file_count > 0u && selected >= 0 && selected < (int)file_count) {
                        snprintf(out_pack_path, out_pack_path_cap, "%s/%s", input_root, files[selected]);
                        done = 1;
                    } else {
                        snprintf(status, sizeof(status), "no .pack file selected");
                    }
                    break;
                default:
                    break;
            }
        }

        {
            int ww = 0;
            int wh = 0;
            SDL_Rect top = {0};
            SDL_Rect list = {0};
            SDL_Rect top_clip = {0};
            SDL_Rect list_clip = {0};
            int pad = 0;
            int line_h1 = 0;
            int line_h2 = 0;
            int row_h = 0;
            int top_gap = 0;
            int list_gap = 0;
            int y_title = 0;
            int y_help = 0;
            int y_path_label = 0;
            int y_path_value = 0;
            int y_status = 0;
            int list_start_y = 0;
            int visible_lines = 0;
            int start_idx = 0;
            SDL_GetRendererOutputSize(renderer, &ww, &wh);
            line_h1 = datalab_text_line_height(1);
            line_h2 = datalab_text_line_height(2);
            pad = datalab_scaled_px(10.0f);
            top_gap = datalab_scaled_px(6.0f);
            list_gap = datalab_scaled_px(4.0f);
            row_h = line_h1 + datalab_scaled_px(4.0f);
            if (row_h < datalab_scaled_px(10.0f)) {
                row_h = datalab_scaled_px(10.0f);
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

            SDL_SetRenderDrawColor(renderer, 11, 12, 16, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 22, 25, 35, 255);
            SDL_RenderFillRect(renderer, &top);
            SDL_SetRenderDrawColor(renderer, 82, 88, 106, 255);
            SDL_RenderDrawRect(renderer, &top);
            SDL_SetRenderDrawColor(renderer, 20, 23, 31, 255);
            SDL_RenderFillRect(renderer, &list);
            SDL_SetRenderDrawColor(renderer, 82, 88, 106, 255);
            SDL_RenderDrawRect(renderer, &list);

            draw_text_5x7(renderer, top.x + pad, y_title,
                          "DATALAB INPUT ROOT + PACK PICKER", 2, 220, 230, 240, 255);
            draw_text_5x7(renderer, top.x + pad, y_help,
                          "E EDIT PATH  ENTER APPLY  B FOLDER DIALOG  UP/DOWN SELECT  ENTER OPEN  ESC CANCEL",
                          1, 170, 185, 205, 255);
            draw_text_5x7(renderer, top.x + pad, y_path_label,
                          edit_mode ? "PATH (EDIT MODE):" : "PATH:",
                          1, 205, 215, 230, 255);
            draw_text_5x7_clipped(renderer,
                                  &top_clip,
                                  top.x + pad,
                                  y_path_value,
                                  edit_mode ? edit_root : input_root,
                                  1,
                                  230,
                                  230,
                                  235,
                                  255);
            draw_text_5x7(renderer, top.x + pad, y_status,
                          "STATUS:", 1, 200, 210, 225, 255);
            draw_text_5x7_clipped(renderer,
                                  &top_clip,
                                  top.x + pad + datalab_scaled_px(58.0f),
                                  y_status,
                                  status,
                                  1,
                                  150,
                                  210,
                                  160,
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
                    SDL_SetRenderDrawColor(renderer, 42, 62, 86, 190);
                    SDL_RenderFillRect(renderer, &hi);
                    SDL_SetRenderDrawColor(renderer, 90, 130, 180, 255);
                    SDL_RenderDrawRect(renderer, &hi);
                }
                draw_text_5x7_clipped(renderer,
                                      &list_clip,
                                      list_clip.x + datalab_scaled_px(6.0f),
                                      text_y,
                                      files[idx],
                                      1,
                                      220,
                                      230,
                                      240,
                                      255);
            }

            if (file_count == 0u) {
                draw_text_5x7_clipped(renderer,
                                      &list_clip,
                                      list_clip.x + datalab_scaled_px(6.0f),
                                      list_start_y + ((row_h - line_h1) / 2),
                                      "NO .PACK FILES FOUND IN INPUT ROOT",
                                      1,
                                      230,
                                      150,
                                      140,
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
    if (canceled) {
        out_pack_path[0] = '\0';
    }
    return core_result_ok();
}
