#include "render/render_view.h"
#include "render/render_view_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kit_workspace_authoring.h"
#include "app/datalab_runtime_prefs.h"
#include "data/input_file_loader.h"
#include "data/pack_inspector.h"
#include "platform/datalab_folder_picker.h"
#include "render/render_view_authoring_overlay_shared.h"
#include "render/render_view_library_preview.h"
#include "render_view_picker_support.h"
#include "render_view_picker_panes.h"

#include "core_pane.h"

#define DATALAB_PICKER_MAX_FILES 256

typedef struct DatalabPickerRecentUiState {
    SDL_Rect list_rect;
    SDL_Rect item_rects[DATALAB_RECENT_INPUT_ROOT_LIMIT];
    size_t visible_count;
    size_t first_index;
} DatalabPickerRecentUiState;

typedef struct DatalabPickerRecentFileUiState {
    SDL_Rect item_rects[4];
    size_t visible_count;
} DatalabPickerRecentFileUiState;


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
    int ignored_startup_quit = 0;
    int received_user_input = 0;
    Uint32 picker_open_ticks = 0u;
    const char *exit_reason = "active";
    int edit_mode = 0;
    int filter_edit_mode = 0;
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
    char all_files[DATALAB_PICKER_MAX_FILES][DATALAB_APP_PATH_CAP];
    char filter[64] = "";
    size_t all_file_count = 0u;
    char recent_input_roots[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP];
    size_t recent_input_root_count = 0u;
    char recent_input_files[DATALAB_RECENT_INPUT_FILE_LIMIT][DATALAB_APP_PATH_CAP];
    size_t recent_input_file_count = 0u;
    char pinned_input_files[DATALAB_RECENT_INPUT_FILE_LIMIT][DATALAB_APP_PATH_CAP];
    size_t pinned_input_file_count = 0u;
    DatalabPickerRecentUiState recent_ui = {0};
    DatalabPickerRecentFileUiState recent_file_ui = {0};
    DatalabPickerScrollState frame_scroll = {0};
    DatalabPickerScrollState directory_scroll = {0};
    int frame_scroll_reveal_selection = 1;
    CorePaneNode picker_panes[5] = {
        { CORE_PANE_NODE_SPLIT, 10u, CORE_PANE_AXIS_HORIZONTAL, 0.22f, 1u, 2u, { 92.0f, 360.0f } },
        { CORE_PANE_NODE_LEAF, 1u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_SPLIT, 20u, CORE_PANE_AXIS_HORIZONTAL, 0.62f, 3u, 4u, { 220.0f, 140.0f } },
        { CORE_PANE_NODE_LEAF, 2u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } },
        { CORE_PANE_NODE_LEAF, 3u, CORE_PANE_AXIS_HORIZONTAL, 0.0f, 0u, 0u, { 0.0f, 0.0f } }
    };
    CorePaneSplitterHit picker_drag_splitter = {0};
    CorePaneSplitterHit picker_splitters[2] = {{0}};
    uint32_t picker_splitter_count = 0u;
    CorePaneRect picker_workspace_bounds = {0};
    int picker_splitter_drag = 0;
    int picker_drag_last_x = 0;
    int picker_drag_last_y = 0;
    DatalabLibraryPreview library_preview = {0};
    DatalabPackInspection pack_inspection = {0};
    char inspected_pack_path[DATALAB_APP_PATH_CAP] = "";
    int pack_inspection_valid = 0;

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
    (void)datalab_runtime_prefs_load_recent_input_files(recent_input_files,
                                                        DATALAB_RECENT_INPUT_FILE_LIMIT,
                                                        &recent_input_file_count);
    (void)datalab_runtime_prefs_load_pinned_input_files(pinned_input_files,
                                                        DATALAB_RECENT_INPUT_FILE_LIMIT,
                                                        &pinned_input_file_count);
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

    all_file_count = datalab_picker_scan_files(input_root,
                                           all_files,
                                           status[0] ? NULL : status,
                                           status[0] ? 0u : sizeof(status));
    file_count = datalab_picker_apply_filter(files, all_files, all_file_count, filter);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    window = SDL_CreateWindow("DataLab | Select Input Root + Pack",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              1160,
                              820,
                              (int)datalab_renderer_backend_window_flags());
    if (!window) {
        SDL_Quit();
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    renderer = datalab_renderer_backend_create(window);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    if (datalab_renderer_backend_kind(renderer) == DATALAB_RENDERER_BACKEND_VULKAN &&
        !datalab_renderer_backend_verify(
            renderer,
            "picker-startup",
            getenv("DATALAB_REQUIRE_VK_VALIDATION") != NULL)) {
        datalab_renderer_backend_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return (CoreResult){ CORE_ERR_IO, "Vulkan picker identity or validation check failed" };
    }
    picker_open_ticks = SDL_GetTicks();

    SDL_StartTextInput();
    while (!done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                Uint32 elapsed_ms = SDL_GetTicks() - picker_open_ticks;
                if (!ignored_startup_quit && !received_user_input) {
                    ignored_startup_quit = 1;
                    snprintf(status, sizeof(status), "ignored pre-input window close event");
                    fprintf(stderr, "datalab picker ignored pre-input SDL_QUIT elapsed_ms=%u\n", elapsed_ms);
                    continue;
                }
                fprintf(stderr, "datalab picker accepted SDL_QUIT elapsed_ms=%u user_input=%d\n", elapsed_ms, received_user_input);
                canceled = 1;
                done = 1;
                exit_reason = "SDL_QUIT";
                break;
            }
            if (e.type == SDL_KEYDOWN || e.type == SDL_TEXTINPUT ||
                e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEWHEEL) {
                received_user_input = 1;
            }
            if (e.type == SDL_TEXTINPUT && (edit_mode || filter_edit_mode)) {
                char *target = edit_mode ? edit_root : filter;
                size_t target_cap = edit_mode ? sizeof(edit_root) : sizeof(filter);
                size_t cur = strlen(target);
                size_t add = strlen(e.text.text);
                if (cur + add + 1u < target_cap) {
                    strncat(target, e.text.text, target_cap - cur - 1u);
                    if (filter_edit_mode) {
                        file_count = datalab_picker_apply_filter(files, all_files, all_file_count, filter);
                        selected = 0;
                        frame_scroll_reveal_selection = 1;
                    }
                }
                continue;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                picker_splitter_drag = 0;
                picker_drag_splitter.active = false;
                datalab_picker_scroll_end_drag(&frame_scroll);
                datalab_picker_scroll_end_drag(&directory_scroll);
                continue;
            }
            if (e.type == SDL_MOUSEMOTION && picker_splitter_drag != 0) {
                int pointer_x = 0;
                int pointer_y = 0;
                if (datalab_render_map_window_to_renderer_point(window, renderer, e.motion.x, e.motion.y, &pointer_x, &pointer_y)) {
                    (void)core_pane_apply_splitter_drag(picker_panes,
                                                        5u,
                                                        &picker_drag_splitter,
                                                        (float)(pointer_x - picker_drag_last_x),
                                                        (float)(pointer_y - picker_drag_last_y));
                    picker_drag_last_x = pointer_x;
                    picker_drag_last_y = pointer_y;
                }
                continue;
            }
            if (e.type == SDL_MOUSEMOTION && (frame_scroll.drag_active || directory_scroll.drag_active)) {
                int pointer_x = 0;
                int pointer_y = 0;
                if (datalab_render_map_window_to_renderer_point(window, renderer, e.motion.x, e.motion.y, &pointer_x, &pointer_y)) {
                    (void)pointer_x;
                    datalab_picker_scroll_drag_to(&frame_scroll, pointer_y);
                    datalab_picker_scroll_drag_to(&directory_scroll, pointer_y);
                }
                continue;
            }
            if (e.type == SDL_MOUSEWHEEL) {
                int pointer_x = 0;
                int pointer_y = 0;
                int window_x = 0;
                int window_y = 0;
                SDL_GetMouseState(&window_x, &window_y);
                if (datalab_render_map_window_to_renderer_point(window, renderer, window_x, window_y, &pointer_x, &pointer_y)) {
                    if (datalab_render_point_in_rect(&frame_scroll.viewport, pointer_x, pointer_y)) {
                        datalab_picker_scroll_by(&frame_scroll, -e.wheel.y * 3);
                        frame_scroll_reveal_selection = 0;
                    } else if (datalab_render_point_in_rect(&directory_scroll.viewport, pointer_x, pointer_y)) {
                        datalab_picker_scroll_by(&directory_scroll, -e.wheel.y * 3);
                    }
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
                if (core_pane_hit_test_splitter_hits(picker_splitters,
                                                     picker_splitter_count,
                                                     (float)pointer_x,
                                                     (float)pointer_y,
                                                     &picker_drag_splitter)) {
                    picker_splitter_drag = 1;
                    picker_drag_last_x = pointer_x;
                    picker_drag_last_y = pointer_y;
                    continue;
                }
                if (datalab_picker_scroll_begin_drag(&frame_scroll, pointer_x, pointer_y)) {
                    frame_scroll_reveal_selection = 0;
                    continue;
                }
                if (datalab_picker_scroll_begin_drag(&directory_scroll, pointer_x, pointer_y)) {
                    continue;
                }
                if (datalab_render_point_in_rect(&frame_scroll.viewport, pointer_x, pointer_y) &&
                    pointer_x < frame_scroll.track.x) {
                    const int row = (pointer_y - frame_scroll.viewport.y) / (frame_scroll.visible_rows > 0 ? frame_scroll.viewport.h / frame_scroll.visible_rows : 1);
                    const int index = frame_scroll.offset_rows + row;
                    if (index >= 0 && index < (int)file_count) {
                        selected = index;
                        frame_scroll_reveal_selection = 1;
                    }
                    continue;
                }
                for (recent_idx = 0u; recent_idx < recent_file_ui.visible_count; ++recent_idx) {
                    if (!datalab_render_point_in_rect(&recent_file_ui.item_rects[recent_idx], pointer_x, pointer_y)) {
                        continue;
                    }
                    if (recent_idx < recent_input_file_count && recent_input_files[recent_idx][0] != '\0') {
                        char recent_root[DATALAB_APP_PATH_CAP];
                        if (!datalab_picker_is_regular_file(recent_input_files[recent_idx])) {
                            snprintf(status, sizeof(status), "recent artifact is no longer available");
                        } else if (!datalab_picker_parent_directory(recent_input_files[recent_idx],
                                                                    recent_root,
                                                                    sizeof(recent_root))) {
                            snprintf(status, sizeof(status), "recent artifact root is unavailable");
                        } else {
                            (void)datalab_input_root_select_recent(input_root,
                                                                   sizeof(input_root),
                                                                   recent_input_roots,
                                                                   &recent_input_root_count,
                                                                   DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                                   recent_root);
                            snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                            snprintf(out_pack_path, out_pack_path_cap, "%s", recent_input_files[recent_idx]);
                            done = 1;
                        }
                        break;
                    }
                }
                if (done) {
                    continue;
                }
                for (recent_idx = 0u; recent_idx < recent_ui.visible_count; ++recent_idx) {
                    if (!datalab_render_point_in_rect(&recent_ui.item_rects[recent_idx], pointer_x, pointer_y)) {
                        continue;
                    }
                    const size_t root_index = recent_ui.first_index + recent_idx;
                    if (root_index < recent_input_root_count && recent_input_roots[root_index][0] != '\0') {
                        (void)datalab_input_root_select_recent(input_root,
                                                               sizeof(input_root),
                                                               recent_input_roots,
                                                               &recent_input_root_count,
                                                               DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                               recent_input_roots[root_index]);
                        snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                        all_file_count = datalab_picker_scan_files(input_root, all_files, status, sizeof(status));
                        file_count = datalab_picker_apply_filter(files, all_files, all_file_count, filter);
                        selected = 0;
                        frame_scroll_reveal_selection = 1;
                        edit_mode = 0;
                    }
                    break;
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
                    case SDLK_t:
                        picker_theme_preset_id = (DatalabWorkspaceAuthoringThemePreset)
                            datalab_workspace_authoring_cycle_runtime_theme_preset(
                                (uint8_t)picker_theme_preset_id,
                                ((e.key.keysym.mod & KMOD_SHIFT) != 0) ? -1 : 1);
                        snprintf(status, sizeof(status), "workspace theme changed");
                        continue;
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
                            exit_reason = "Escape";
                    }
                    break;
                case SDLK_e:
                    edit_mode = !edit_mode;
                    if (edit_mode) {
                        snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                    }
                    break;
                case SDLK_SLASH:
                    if (!edit_mode) filter_edit_mode = !filter_edit_mode;
                    break;
                case SDLK_p:
                    if (!edit_mode && !filter_edit_mode && file_count > 0u && selected >= 0 && selected < (int)file_count) {
                        char selected_full_path[DATALAB_APP_PATH_CAP]; size_t found = pinned_input_file_count;
                        if (!datalab_input_root_join_child_file(input_root, files[selected], selected_full_path, sizeof(selected_full_path))) break;
                        for (size_t i = 0u; i < pinned_input_file_count; ++i) if (strcmp(pinned_input_files[i], selected_full_path) == 0) { found = i; break; }
                        if (found < pinned_input_file_count) {
                            for (size_t i = found + 1u; i < pinned_input_file_count; ++i) snprintf(pinned_input_files[i - 1u], DATALAB_APP_PATH_CAP, "%s", pinned_input_files[i]);
                            pinned_input_file_count--; snprintf(status, sizeof(status), "artifact pin removed");
                        } else if (pinned_input_file_count < DATALAB_RECENT_INPUT_FILE_LIMIT) {
                            snprintf(pinned_input_files[pinned_input_file_count++], DATALAB_APP_PATH_CAP, "%s", selected_full_path);
                            snprintf(status, sizeof(status), "artifact pinned");
                        }
                        (void)datalab_runtime_prefs_save_pinned_input_files(pinned_input_files, pinned_input_file_count);
                    }
                    break;
                case SDLK_b:
                    if (!edit_mode) {
                        char picked[DATALAB_APP_PATH_CAP];
                        if (Datalab_FolderPicker_Select("Choose DataLab Input Folder",
                                                        input_root,
                                                        picked,
                                                        sizeof(picked)) == DATALAB_FOLDER_PICKER_SELECTED) {
                            (void)datalab_input_root_select_recent(input_root,
                                                                   sizeof(input_root),
                                                                   recent_input_roots,
                                                                   &recent_input_root_count,
                                                                   DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                                   picked);
                            snprintf(edit_root, sizeof(edit_root), "%s", input_root);
                            all_file_count = datalab_picker_scan_files(input_root, all_files, status, sizeof(status));
                            file_count = datalab_picker_apply_filter(files, all_files, all_file_count, filter);
                            selected = 0;
                            frame_scroll_reveal_selection = 1;
                        } else {
                            snprintf(status, sizeof(status), "folder dialog canceled/unavailable");
                        }
                    }
                    break;
                case SDLK_BACKSPACE:
                    if (filter_edit_mode) {
                        size_t len = strlen(filter);
                        if (len > 0u) {
                            filter[len - 1u] = '\0';
                            file_count = datalab_picker_apply_filter(files, all_files, all_file_count, filter);
                            selected = 0;
                            frame_scroll_reveal_selection = 1;
                        }
                    } else if (edit_mode) {
                        size_t len = strlen(edit_root);
                        if (len > 0u) {
                            edit_root[len - 1u] = '\0';
                        }
                    }
                    break;
                case SDLK_UP:
                    if (!edit_mode && selected > 0) {
                        selected--;
                        frame_scroll_reveal_selection = 1;
                    }
                    break;
                case SDLK_DOWN:
                    if (!edit_mode && selected + 1 < (int)file_count) {
                        selected++;
                        frame_scroll_reveal_selection = 1;
                    }
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (edit_mode) {
                        if (!datalab_picker_is_directory(edit_root)) {
                            snprintf(status, sizeof(status), "invalid directory: %s", edit_root);
                            break;
                        }
                        (void)datalab_input_root_select_recent(input_root,
                                                               sizeof(input_root),
                                                               recent_input_roots,
                                                               &recent_input_root_count,
                                                               DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                               edit_root);
                        all_file_count = datalab_picker_scan_files(input_root, all_files, status, sizeof(status));
                        file_count = datalab_picker_apply_filter(files, all_files, all_file_count, filter);
                        selected = 0;
                        frame_scroll_reveal_selection = 1;
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
            SDL_Rect browser = {0};
            SDL_Rect preview = {0};
            SDL_Rect directories = {0};
            SDL_Rect top_clip = {0};
            SDL_Rect list_clip = {0};
            SDL_Rect frame_rows_clip = {0};
            CorePaneLeafRect picker_leaves[3] = {{0}};
            uint32_t picker_leaf_count = 0u;
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
            int recent_file_rows = 0;
            int recent_file_y = 0;
            char selected_path[DATALAB_APP_PATH_CAP] = "";
            char display_input_root[DATALAB_APP_PATH_CAP] = "";
            char display_edit_root[DATALAB_APP_PATH_CAP] = "";
            char display_recent_roots[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP] = {{0}};
            char preview_path[DATALAB_APP_PATH_CAP] = "";
            char preview_label[160] = "DIRECTORY IMAGE PREVIEW";
            char selected_detail[192] = "SELECT AN ARTIFACT TO INSPECT";
            datalab_renderer_backend_output_size(renderer, &ww, &wh);
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
            picker_workspace_bounds = (CorePaneRect){ (float)list.x, (float)list.y, (float)list.w, (float)list.h };
            picker_panes[0].constraints.min_size_a = (float)datalab_scaled_px(92.0f);
            picker_panes[0].constraints.min_size_b = (float)datalab_scaled_px(360.0f);
            picker_panes[2].constraints.min_size_a = (float)datalab_scaled_px(220.0f);
            picker_panes[2].constraints.min_size_b = (float)datalab_scaled_px(140.0f);
            if (core_pane_solve(picker_panes, 5u, 0u, picker_workspace_bounds,
                                picker_leaves, 3u, &picker_leaf_count) && picker_leaf_count == 3u) {
                browser = (SDL_Rect){ (int)picker_leaves[0].rect.x, (int)picker_leaves[0].rect.y,
                                      (int)picker_leaves[0].rect.width, (int)picker_leaves[0].rect.height };
                preview = (SDL_Rect){ (int)picker_leaves[1].rect.x, (int)picker_leaves[1].rect.y,
                                      (int)picker_leaves[1].rect.width, (int)picker_leaves[1].rect.height };
                directories = (SDL_Rect){ (int)picker_leaves[2].rect.x, (int)picker_leaves[2].rect.y,
                                          (int)picker_leaves[2].rect.width, (int)picker_leaves[2].rect.height };
            } else {
                browser = (SDL_Rect){ list.x, list.y, list.w / 5, list.h };
                preview = (SDL_Rect){ browser.x + browser.w, list.y, (list.w * 3) / 5, list.h };
                directories = (SDL_Rect){ preview.x + preview.w, list.y, list.x + list.w - (preview.x + preview.w), list.h };
            }
            (void)core_pane_collect_splitter_hits(picker_panes, 5u, 0u, picker_workspace_bounds,
                                                  (float)datalab_scaled_px(16.0f), picker_splitters, 2u,
                                                  &picker_splitter_count);
            top_clip.x = top.x + pad;
            top_clip.y = top.y + datalab_scaled_px(6.0f);
            top_clip.w = top.w - (pad * 2);
            top_clip.h = top.h - datalab_scaled_px(12.0f);
            list_clip.x = browser.x + datalab_scaled_px(6.0f);
            list_clip.y = browser.y + datalab_scaled_px(4.0f);
            list_clip.w = browser.w - datalab_scaled_px(8.0f);
            list_clip.h = browser.h - datalab_scaled_px(8.0f);
            list_start_y = browser.y + list_gap;

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
            SDL_SetRenderDrawColor(renderer, palette.frame_r, palette.frame_g, palette.frame_b,
                                   picker_splitter_drag ? 255 : 210);
            for (uint32_t splitter_i = 0u; splitter_i < picker_splitter_count; ++splitter_i) {
                const int divider_x = (int)(picker_splitters[splitter_i].splitter_bounds.x +
                                            (picker_splitters[splitter_i].splitter_bounds.width * 0.5f));
                SDL_RenderDrawLine(renderer, divider_x, list.y, divider_x, list.y + list.h - 1);
            }

            if (file_count > 0u && selected >= 0 && selected < (int)file_count) {
                struct stat selected_stat;
                if (datalab_input_root_join_child_file(input_root, files[selected], selected_path, sizeof(selected_path)) &&
                    stat(selected_path, &selected_stat) == 0) {
                    const char *kind = datalab_input_file_is_pack(selected_path) ? "PACK" :
                                       datalab_input_file_is_png(selected_path) ? "PNG IMAGE" : "BMP IMAGE";
                    snprintf(selected_detail, sizeof(selected_detail), "%s%s  |  %lld BYTES  |  %s",
                             datalab_picker_path_is_pinned((const char (*)[DATALAB_APP_PATH_CAP])pinned_input_files, pinned_input_file_count, selected_path) ? "PINNED " : "",
                             kind, (long long)selected_stat.st_size, files[selected]);
                    if (datalab_input_file_is_png(selected_path) || datalab_input_file_is_bmp(selected_path)) {
                        snprintf(preview_path, sizeof(preview_path), "%s", selected_path);
                        snprintf(preview_label, sizeof(preview_label), "SELECTED IMAGE PREVIEW");
                    }
                }
            }
            if (datalab_input_file_is_pack(selected_path)) {
                if (strcmp(inspected_pack_path, selected_path) != 0) {
                    pack_inspection_valid = datalab_inspect_pack(selected_path, &pack_inspection).code == CORE_OK;
                    snprintf(inspected_pack_path, sizeof(inspected_pack_path), "%s", selected_path);
                }
            }
            if (preview_path[0] == '\0') {
                for (size_t preview_i = 0u; preview_i < file_count; ++preview_i) {
                    if (!datalab_input_file_is_png(files[preview_i]) && !datalab_input_file_is_bmp(files[preview_i])) {
                        continue;
                    }
                    if (datalab_input_root_join_child_file(input_root,
                                                           files[preview_i],
                                                           preview_path,
                                                           sizeof(preview_path))) {
                        break;
                    }
                }
            }
            datalab_library_preview_prepare(renderer, &library_preview, preview_path);

            SDL_SetRenderDrawColor(renderer, palette.top_fill_r, palette.top_fill_g, palette.top_fill_b, 255);
            SDL_RenderFillRect(renderer, &directories);
            SDL_SetRenderDrawColor(renderer, palette.frame_r, palette.frame_g, palette.frame_b, 255);
            SDL_RenderDrawRect(renderer, &directories);
            draw_text_5x7(renderer, directories.x + pad, directories.y + pad,
                          "RECENT DIRECTORIES", 1,
                          palette.text_primary_r, palette.text_primary_g, palette.text_primary_b, 255);
            draw_text_5x7(renderer, directories.x + pad, directories.y + pad + line_h1 + datalab_scaled_px(2.0f),
                          "CLICK TO LOAD ROOT", 1,
                          palette.text_muted_r, palette.text_muted_g, palette.text_muted_b, 255);
            directory_scroll.viewport = (SDL_Rect){
                directories.x + pad,
                directories.y + (pad * 2) + (line_h1 * 2),
                directories.w - (pad * 2),
                directories.h - ((pad * 3) + (line_h1 * 2))
            };
            datalab_picker_scroll_configure(&directory_scroll,
                                            directory_scroll.viewport,
                                            recent_visible,
                                            directory_scroll.viewport.h / recent_row_h,
                                            recent_row_h);
            recent_ui.list_rect = directory_scroll.viewport;
            recent_ui.first_index = (size_t)directory_scroll.offset_rows;
            recent_ui.visible_count = (size_t)directory_scroll.visible_rows;
            if (recent_ui.visible_count > (size_t)recent_visible) recent_ui.visible_count = (size_t)recent_visible;
            for (size_t recent_i = 0u; recent_i < recent_ui.visible_count; ++recent_i) {
                const size_t root_index = recent_ui.first_index + recent_i;
                SDL_Rect item_rect = {
                    directories.x + pad,
                    directory_scroll.viewport.y + ((int)recent_i * recent_row_h),
                    directory_scroll.viewport.w - datalab_scaled_px(6.0f), recent_row_h
                };
                int is_active_recent = root_index < recent_input_root_count && strcmp(recent_input_roots[root_index], input_root) == 0;
                if (is_active_recent) {
                    SDL_SetRenderDrawColor(renderer, palette.selected_fill_r, palette.selected_fill_g, palette.selected_fill_b, 200);
                    SDL_RenderFillRect(renderer, &item_rect);
                    SDL_SetRenderDrawColor(renderer, palette.selected_border_r, palette.selected_border_g, palette.selected_border_b, 255);
                    SDL_RenderDrawRect(renderer, &item_rect);
                }
                draw_text_5x7_clipped(renderer, &directory_scroll.viewport, item_rect.x + datalab_scaled_px(4.0f), item_rect.y + datalab_scaled_px(2.0f),
                                      datalab_picker_display_path(recent_input_roots[root_index], display_recent_roots[recent_i], sizeof(display_recent_roots[recent_i])), 1,
                                      is_active_recent ? palette.text_primary_r : palette.text_secondary_r,
                                      is_active_recent ? palette.text_primary_g : palette.text_secondary_g,
                                      is_active_recent ? palette.text_primary_b : palette.text_secondary_b, 255);
                recent_ui.item_rects[recent_i] = item_rect;
            }
            datalab_picker_scroll_draw(renderer, &directory_scroll, &palette);

            draw_text_5x7(renderer, top.x + pad, y_title,
                          "DATALAB LIBRARY", 2,
                          palette.text_primary_r, palette.text_primary_g, palette.text_primary_b, 255);
            draw_text_5x7(renderer, top.x + pad, y_help,
                          "TECHNICAL WORKSPACE  |  PACK + BMP + PNG  |  CMD/CTRL+T THEME  / FILTER  B BROWSE  E EDIT LOCATION  ENTER OPEN",
                          1, palette.text_secondary_r, palette.text_secondary_g, palette.text_secondary_b, 255);
            draw_text_5x7(renderer, top.x + pad, y_path_label,
                          edit_mode ? "PATH (EDIT MODE):" : "PATH:",
                          1, palette.text_muted_r, palette.text_muted_g, palette.text_muted_b, 255);
            draw_text_5x7_clipped(renderer,
                                  &top_clip,
                                  top.x + pad,
                                  y_path_value,
                                  edit_mode ? datalab_picker_display_path(edit_root, display_edit_root, sizeof(display_edit_root)) : datalab_picker_display_path(input_root, display_input_root, sizeof(display_input_root)),
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
                                  status[0] ? status : selected_detail,
                                  1,
                                  palette.text_success_r,
                                  palette.text_success_g,
                                  palette.text_success_b,
                                  255);
            if (filter[0] != '\0' || filter_edit_mode) {
                char filter_label[96];
                snprintf(filter_label, sizeof(filter_label), "FILTER%s: %s (%zu/%zu)", filter_edit_mode ? " EDIT" : "", filter[0] ? filter : "ALL", file_count, all_file_count);
                draw_text_5x7_clipped(renderer, &top_clip, top.x + (top.w / 2), y_status, filter_label, 1,
                                      palette.text_secondary_r, palette.text_secondary_g, palette.text_secondary_b, 255);
            }

            recent_file_rows = (int)recent_input_file_count;
            if (recent_file_rows > 4) {
                recent_file_rows = 4;
            }
            recent_file_y = browser.y + browser.h - (recent_file_rows > 0 ? (recent_file_rows * row_h) + line_h1 + (pad * 2) : 0);
            if (recent_file_rows > 0) {
                recent_file_ui.visible_count = (size_t)recent_file_rows;
                draw_text_5x7(renderer, browser.x + pad, recent_file_y,
                              "RECENT ARTIFACTS", 1,
                              palette.text_secondary_r, palette.text_secondary_g, palette.text_secondary_b, 255);
                for (int recent_file_i = 0; recent_file_i < recent_file_rows; ++recent_file_i) {
                    recent_file_ui.item_rects[recent_file_i] = (SDL_Rect){
                        browser.x + pad,
                        recent_file_y + line_h1 + datalab_scaled_px(4.0f) + (recent_file_i * row_h),
                        browser.w - (pad * 2), row_h
                    };
                    draw_text_5x7_clipped(renderer, &list_clip,
                                          browser.x + pad,
                                          recent_file_y + line_h1 + datalab_scaled_px(4.0f) + (recent_file_i * row_h),
                                          recent_input_files[recent_file_i], 1,
                                          palette.text_muted_r, palette.text_muted_g, palette.text_muted_b, 255);
                }
            }
            visible_lines = ((recent_file_rows > 0 ? recent_file_y : list.y + list.h) - list_start_y - (list_gap * 2)) / row_h;
            if (visible_lines < 1) {
                visible_lines = 1;
            }
            frame_scroll.viewport = (SDL_Rect){ list_clip.x, list_start_y, list_clip.w,
                                                visible_lines * row_h };
            datalab_picker_scroll_configure(&frame_scroll, frame_scroll.viewport, (int)file_count,
                                            visible_lines, row_h);
            if (frame_scroll_reveal_selection) {
                if (selected < frame_scroll.offset_rows) frame_scroll.offset_rows = selected;
                if (selected >= frame_scroll.offset_rows + visible_lines) frame_scroll.offset_rows = selected - visible_lines + 1;
                frame_scroll_reveal_selection = 0;
                datalab_picker_scroll_configure(&frame_scroll, frame_scroll.viewport, (int)file_count,
                                                visible_lines, row_h);
            }
            frame_rows_clip = frame_scroll.viewport;
            if (frame_scroll.content_rows > frame_scroll.visible_rows) {
                frame_rows_clip.w = frame_scroll.track.x - frame_rows_clip.x - datalab_scaled_px(2.0f);
            }
            if (frame_rows_clip.w < 1) frame_rows_clip.w = 1;
            start_idx = frame_scroll.offset_rows;
            for (int i = 0; i < visible_lines; ++i) {
                int idx = start_idx + i;
                int row_y = list_start_y + i * row_h;
                int text_y = row_y + ((row_h - line_h1) / 2);
                if (idx >= (int)file_count) {
                    break;
                }
                if (idx == selected) {
                    SDL_Rect hi = { frame_rows_clip.x, row_y, frame_rows_clip.w, row_h };
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
                                      &frame_rows_clip,
                                      frame_rows_clip.x + datalab_scaled_px(6.0f),
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
                                      &frame_rows_clip,
                                      frame_rows_clip.x + datalab_scaled_px(6.0f),
                                      list_start_y + ((row_h - line_h1) / 2),
                                      "NO SUPPORTED FILES (.PACK/.BMP/.PNG) FOUND IN INPUT ROOT",
                                      1,
                                      palette.text_empty_r,
                                      palette.text_empty_g,
                                      palette.text_empty_b,
                                      255);
            }
            datalab_picker_scroll_draw(renderer, &frame_scroll, &palette);
            datalab_picker_scroll_draw_selection_marker(renderer, &frame_scroll, selected, &palette);
            if (datalab_input_file_is_pack(selected_path)) {
                datalab_picker_draw_pack_inspection(renderer,
                                                    &preview,
                                                    &palette,
                                                    &pack_inspection,
                                                    pack_inspection_valid);
            } else {
                DatalabLibraryPreviewColors preview_colors = {
                    palette.top_fill_r, palette.top_fill_g, palette.top_fill_b,
                    palette.frame_r, palette.frame_g, palette.frame_b,
                    palette.text_primary_r, palette.text_primary_g, palette.text_primary_b,
                    palette.text_muted_r, palette.text_muted_g, palette.text_muted_b
                };
                datalab_library_preview_draw(renderer,
                                             &library_preview,
                                             &preview,
                                             &preview_colors,
                                             preview_label);
            }
            (void)datalab_renderer_backend_present(renderer);
            if (getenv("DATALAB_PICKER_PROOF_EXIT") != NULL) {
                canceled = 1;
                done = 1;
                exit_reason = "proof-exit";
            }
        }
    }

    SDL_StopTextInput();
    datalab_library_preview_destroy(&library_preview);
    datalab_renderer_backend_destroy(renderer);
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
    if (!canceled && out_pack_path[0] != '\0') {
        datalab_recent_input_files_add(recent_input_files,
                                       &recent_input_file_count,
                                       DATALAB_RECENT_INPUT_FILE_LIMIT,
                                       out_pack_path);
        (void)datalab_runtime_prefs_save_recent_input_files(recent_input_files, recent_input_file_count);
    }
    if (canceled) {
        out_pack_path[0] = '\0';
    }
    fprintf(stderr,
            "datalab picker exit reason=%s canceled=%d selected=%d files=%zu\n",
            exit_reason,
            canceled,
            selected,
            file_count);
    return core_result_ok();
}
