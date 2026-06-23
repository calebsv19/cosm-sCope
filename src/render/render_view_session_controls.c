#include "render/render_view_internal.h"

#include <stdio.h>
#include <string.h>

#include "kit_ui_sdl.h"
#include "render/render_view_authoring_overlay_shared.h"

#define DATALAB_PANEL_REFRESH_MS 1200u

typedef struct DatalabRecentInputRootUiState {
    SDL_Rect button_rect;
    SDL_Rect list_rect;
    SDL_Rect item_rects[DATALAB_RECENT_INPUT_ROOT_LIMIT];
    size_t visible_count;
} DatalabRecentInputRootUiState;

static DatalabPackPanelCache g_pack_panel_cache;
static DatalabRecentInputRootUiState g_recent_input_root_ui;

static int datalab_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static KitRenderColor datalab_session_hud_text_color(const KitUiHudStyle *style, int muted) {
    if (!style) {
        return (KitRenderColor){220u, 224u, 232u, 255u};
    }
    return muted ? style->text_disabled : style->text;
}

static int datalab_header_bar_height_px(void) {
    return (datalab_text_line_height(1) * 2) + datalab_scaled_px(12.0f);
}

static void datalab_panel_rescan(const char *root, DatalabPackPanelCache *cache) {
    DatalabSupportedFileScanResult scan = {0u, 0, 0};
    if (!root || !cache) {
        return;
    }
    cache->file_count = 0u;
    cache->status[0] = '\0';
    snprintf(cache->scanned_root, sizeof(cache->scanned_root), "%s", root);
    scan = datalab_scan_supported_files(root, cache->files, DATALAB_PANEL_MAX_FILES);
    cache->file_count = scan.file_count;
    datalab_format_supported_file_scan_status(&scan,
                                              root,
                                              "press O to reselect",
                                              cache->status,
                                              sizeof(cache->status));
}

void datalab_session_controls_tick(DatalabAppState *app_state) {
    const char *root = NULL;
    uint32_t now_ticks = 0u;
    int rescanned = 0;
    if (!app_state) {
        return;
    }
    root = app_state->input_root;
    if (root[0] == '\0') {
        datalab_panel_apply_state(app_state, &g_pack_panel_cache, root, 0, 0u);
        return;
    }
    now_ticks = SDL_GetTicks();
    if (app_state->panel_rescan_requested ||
        strncmp(g_pack_panel_cache.scanned_root, root, sizeof(g_pack_panel_cache.scanned_root)) != 0 ||
        (now_ticks - g_pack_panel_cache.last_scan_ticks) > DATALAB_PANEL_REFRESH_MS) {
        datalab_panel_rescan(root, &g_pack_panel_cache);
        g_pack_panel_cache.last_scan_ticks = now_ticks;
        app_state->panel_rescan_requested = 0;
        rescanned = 1;
    }
    datalab_panel_apply_state(app_state, &g_pack_panel_cache, root, rescanned, now_ticks);
}

static void datalab_recent_input_root_activate(DatalabAppState *app_state, const char *path) {
    uint32_t now_ticks = 0u;
    if (!app_state || !path || path[0] == '\0') {
        return;
    }
    if (!datalab_app_state_select_input_root(app_state, path)) {
        return;
    }
    datalab_panel_rescan(app_state->input_root, &g_pack_panel_cache);
    now_ticks = SDL_GetTicks();
    g_pack_panel_cache.last_scan_ticks = now_ticks;
    if (g_pack_panel_cache.file_count > 0u) {
        datalab_panel_request_pack_under_root(app_state,
                                              app_state->input_root,
                                              g_pack_panel_cache.files[0]);
    }
}

int datalab_session_controls_mouse_enabled(const DatalabAppState *app_state) {
    if (!app_state) {
        return 0;
    }
    if (app_state->workspace_authoring_stub_active) {
        return 0;
    }
    return 1;
}

size_t datalab_session_controls_file_count(void) {
    return g_pack_panel_cache.file_count;
}

const char *datalab_session_controls_selected_file_name(const DatalabAppState *app_state) {
    if (!app_state || g_pack_panel_cache.file_count == 0u ||
        app_state->panel_selected_index >= g_pack_panel_cache.file_count) {
        return "";
    }
    return g_pack_panel_cache.files[app_state->panel_selected_index];
}

int datalab_session_controls_route_mouse_event(SDL_Window *window,
                                               SDL_Renderer *renderer,
                                               const SDL_Event *event,
                                               DatalabAppState *app_state) {
    int pointer_x = 0;
    int pointer_y = 0;
    size_t i = 0u;
    if (!window || !renderer || !event || !app_state) {
        return 0;
    }
    if (!datalab_session_controls_mouse_enabled(app_state)) {
        return 0;
    }
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT) {
        return 0;
    }
    if (!datalab_render_map_window_to_renderer_point(window,
                                                     renderer,
                                                     event->button.x,
                                                     event->button.y,
                                                     &pointer_x,
                                                     &pointer_y)) {
        return 0;
    }
    if (datalab_render_point_in_rect(&g_recent_input_root_ui.button_rect, pointer_x, pointer_y)) {
        app_state->recent_input_root_dropdown_open = !app_state->recent_input_root_dropdown_open;
        return 1;
    }
    if (!app_state->recent_input_root_dropdown_open) {
        return 0;
    }
    for (i = 0u; i < g_recent_input_root_ui.visible_count; ++i) {
        if (!datalab_render_point_in_rect(&g_recent_input_root_ui.item_rects[i], pointer_x, pointer_y)) {
            continue;
        }
        if (i < app_state->recent_input_root_count && app_state->recent_input_roots[i][0] != '\0') {
            datalab_recent_input_root_activate(app_state, app_state->recent_input_roots[i]);
            return 1;
        }
    }
    if (!datalab_render_point_in_rect(&g_recent_input_root_ui.list_rect, pointer_x, pointer_y)) {
        app_state->recent_input_root_dropdown_open = 0;
        return 1;
    }
    return 0;
}

void datalab_draw_recent_input_root_header(SDL_Renderer *renderer, const DatalabAppState *app_state) {
    DatalabAuthoringThemePalette palette = {0};
    DatalabWorkspaceAuthoringThemePreset preset = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    SDL_Rect bar = {0};
    SDL_Rect button = {0};
    SDL_Rect list_rect = {0};
    SDL_Rect clip_rect = {0};
    const char *root = NULL;
    int ww = 0;
    int wh = 0;
    int pad = 0;
    int line_h = 0;
    int button_w = 0;
    int button_h = 0;
    int row_h = 0;
    int visible_count = 0;
    size_t i = 0u;

    if (!renderer || !app_state) {
        return;
    }
    memset(&g_recent_input_root_ui, 0, sizeof(g_recent_input_root_ui));
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    if (ww <= 0 || wh <= 0) {
        return;
    }

    preset = datalab_overlay_selected_theme(app_state);
    datalab_overlay_theme_palette(preset, &app_state->workspace_authoring_custom_theme, &palette);
    root = (app_state->input_root[0] != '\0') ? app_state->input_root : "<not set>";
    pad = datalab_scaled_px(8.0f);
    line_h = datalab_text_line_height(1);
    button_w = datalab_scaled_px(280.0f);
    button_h = (line_h * 2) + datalab_scaled_px(8.0f);
    row_h = line_h + datalab_scaled_px(4.0f);
    visible_count = (int)app_state->recent_input_root_count;
    if (visible_count > DATALAB_RECENT_INPUT_ROOT_LIMIT) {
        visible_count = DATALAB_RECENT_INPUT_ROOT_LIMIT;
    }

    bar = (SDL_Rect){0, 0, ww, button_h + datalab_scaled_px(4.0f)};
    button = (SDL_Rect){
        ww - button_w - datalab_scaled_px(12.0f),
        datalab_scaled_px(4.0f),
        button_w,
        button_h
    };
    if (button.x < datalab_scaled_px(120.0f)) {
        button.x = datalab_scaled_px(120.0f);
        button.w = ww - button.x - datalab_scaled_px(12.0f);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, palette.shell_fill_r, palette.shell_fill_g, palette.shell_fill_b, 228);
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, palette.shell_border_r, palette.shell_border_g, palette.shell_border_b, 240);
    SDL_RenderDrawLine(renderer, 0, bar.h - 1, ww, bar.h - 1);

    draw_text_5x7(renderer,
                  datalab_scaled_px(12.0f),
                  datalab_scaled_px(10.0f),
                  "DATALAB",
                  1,
                  palette.text_primary_r,
                  palette.text_primary_g,
                  palette.text_primary_b,
                  255);

    datalab_overlay_draw_button(renderer,
                                &button,
                                "RECENT DIRECTORIES",
                                app_state->recent_input_root_dropdown_open,
                                app_state->recent_input_root_dropdown_open,
                                &palette);
    clip_rect = (SDL_Rect){
        button.x + pad,
        button.y + line_h + datalab_scaled_px(2.0f),
        button.w - (pad * 2),
        line_h
    };
    draw_text_5x7_clipped(renderer,
                          &clip_rect,
                          clip_rect.x,
                          clip_rect.y,
                          root,
                          1,
                          palette.text_secondary_r,
                          palette.text_secondary_g,
                          palette.text_secondary_b,
                          255);
    g_recent_input_root_ui.button_rect = button;

    if (app_state->recent_input_root_dropdown_open && visible_count > 0) {
        list_rect = (SDL_Rect){
            button.x,
            button.y + button.h + datalab_scaled_px(4.0f),
            button.w,
            (visible_count * row_h) + (pad * 2)
        };
        SDL_SetRenderDrawColor(renderer, palette.pane_fill_r, palette.pane_fill_g, palette.pane_fill_b, 238);
        SDL_RenderFillRect(renderer, &list_rect);
        SDL_SetRenderDrawColor(renderer, palette.shell_border_r, palette.shell_border_g, palette.shell_border_b, 245);
        SDL_RenderDrawRect(renderer, &list_rect);
        clip_rect = (SDL_Rect){
            list_rect.x + pad,
            list_rect.y + pad,
            list_rect.w - (pad * 2),
            list_rect.h - (pad * 2)
        };
        g_recent_input_root_ui.list_rect = list_rect;
        g_recent_input_root_ui.visible_count = (size_t)visible_count;
        for (i = 0u; i < (size_t)visible_count; ++i) {
            SDL_Rect item_rect = {
                list_rect.x + pad,
                list_rect.y + pad + ((int)i * row_h),
                list_rect.w - (pad * 2),
                row_h
            };
            int is_active = strcmp(app_state->recent_input_roots[i], root) == 0;
            if (is_active) {
                SDL_SetRenderDrawColor(renderer, palette.button_active_r, palette.button_active_g, palette.button_active_b, 230);
                SDL_RenderFillRect(renderer, &item_rect);
            }
            draw_text_5x7_clipped(renderer,
                                  &clip_rect,
                                  item_rect.x + datalab_scaled_px(4.0f),
                                  item_rect.y + datalab_scaled_px(2.0f),
                                  app_state->recent_input_roots[i],
                                  1,
                                  is_active ? palette.text_primary_r : palette.text_secondary_r,
                                  is_active ? palette.text_primary_g : palette.text_secondary_g,
                                  is_active ? palette.text_primary_b : palette.text_secondary_b,
                                  255);
            g_recent_input_root_ui.item_rects[i] = item_rect;
        }
    }
}

void datalab_draw_session_controls(SDL_Renderer *renderer, const DatalabAppState *app_state) {
    DatalabAuthoringThemePalette palette = {0};
    DatalabWorkspaceAuthoringThemePreset preset = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    KitUiHudStyle hud_style;
    KitRenderColor primary_text = {0};
    KitRenderColor muted_text = {0};
    KitRenderColor error_text = {230u, 150u, 140u, 255u};
    SDL_Rect panel = {0};
    SDL_Rect panel_clip = {0};
    SDL_Rect list_box = {0};
    const char *pack_path = NULL;
    const char *active_name = NULL;
    const char *root = NULL;
    int ww = 0;
    int wh = 0;
    int pad = 0;
    int section_gap = 0;
    int line_h = 0;
    int content_w = 0;
    int measured_w = 0;
    int measured_h = 0;
    int y_cursor = 0;
    int min_panel_w = 0;
    int max_panel_w = 0;
    int min_panel_h = 0;
    int max_panel_h = 0;
    int row_h = 0;
    int max_rows = 0;
    int start_y = 0;
    int panel_radius = 0;
    int list_radius = 0;
    int row_radius = 0;
    size_t start_idx = 0u;
    const char *shortcut_line = NULL;
    if (!renderer || !app_state) {
        return;
    }
    preset = datalab_overlay_selected_theme(app_state);
    datalab_overlay_theme_palette(preset, &app_state->workspace_authoring_custom_theme, &palette);
    datalab_overlay_hud_style_from_palette(&palette, &hud_style);
    hud_style.panel_corner_radius = (float)datalab_scaled_px(hud_style.panel_corner_radius);
    root = (app_state->input_root[0] != '\0') ? app_state->input_root : "<not set>";
    pack_path = app_state->pack_path && app_state->pack_path[0] ? app_state->pack_path : "<none>";
    active_name = core_path_basename(pack_path);
    shortcut_line = datalab_profile_supports_raster_viewport(app_state->profile)
                        ? "H hide HUD | Wheel zoom | Left drag pan | R reset | Space play/pause | O picker | U/J nav | Enter load | Left/Right cycle image | F5 rescan"
                        : "H hide HUD | Space play/pause | O picker | U/J nav | Enter load | Left/Right cycle image | F5 rescan";
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    pad = datalab_scaled_px(8.0f);
    if (app_state->session_hud_collapsed) {
        return;
    }

    section_gap = datalab_scaled_px(4.0f);
    line_h = datalab_text_line_height(1);
    min_panel_w = datalab_scaled_px(260.0f);
    max_panel_w = ww - datalab_scaled_px(20.0f);
    if (max_panel_w < min_panel_w) {
        max_panel_w = min_panel_w;
    }
    min_panel_h = datalab_scaled_px(200.0f);
    max_panel_h = datalab_clamp_int((wh * 50) / 100, datalab_scaled_px(220.0f), datalab_scaled_px(380.0f));

    (void)datalab_measure_text(1, "SESSION DATA", &content_w, &measured_h);
    (void)datalab_measure_text(1, shortcut_line, &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;
    (void)datalab_measure_text(1, "ROOT", &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;
    (void)datalab_measure_text(1, "ACTIVE", &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;
    (void)datalab_measure_text(1, root, &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;
    (void)datalab_measure_text(1, pack_path, &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;
    (void)datalab_measure_text(1, g_pack_panel_cache.status[0] ? g_pack_panel_cache.status : "scanning...", &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;
    (void)datalab_measure_text(1, "NO SUPPORTED FILES (.PACK/.BMP) IN INPUT ROOT", &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;

    panel.x = datalab_scaled_px(10.0f);
    panel.y = datalab_header_bar_height_px() + datalab_scaled_px(10.0f);
    panel.w = datalab_clamp_int(content_w + (pad * 2) + datalab_scaled_px(8.0f), min_panel_w, max_panel_w);
    panel.h = datalab_clamp_int((line_h * 7) + (section_gap * 6) + datalab_scaled_px(120.0f), min_panel_h, max_panel_h);
    panel_radius = (int)kit_ui_corner_radius_clamp(hud_style.panel_corner_radius,
                                                   (float)panel.w,
                                                   (float)panel.h);
    list_radius = (int)kit_ui_corner_radius_clamp(kit_ui_corner_radius_for_inset(hud_style.panel_corner_radius,
                                                                                 (float)pad),
                                                  (float)(panel.w - (pad * 2)),
                                                  (float)(panel.h - (pad * 2)));
    hud_style.button_corner_radius = (float)list_radius;
    primary_text = datalab_session_hud_text_color(&hud_style, 0);
    muted_text = datalab_session_hud_text_color(&hud_style, 1);
    kit_ui_sdl_fill_rounded_rect(renderer, &panel, panel_radius, hud_style.panel_fill);
    panel_clip.x = panel.x + pad;
    panel_clip.y = panel.y + pad;
    panel_clip.w = panel.w - (pad * 2);
    panel_clip.h = panel.h - (pad * 2);

    y_cursor = panel.y + pad;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  "SESSION DATA",
                  1,
                  primary_text.r,
                  primary_text.g,
                  primary_text.b,
                  primary_text.a);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  shortcut_line,
                  1,
                  muted_text.r,
                  muted_text.g,
                  muted_text.b,
                  muted_text.a);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  "ROOT",
                  1,
                  muted_text.r,
                  muted_text.g,
                  muted_text.b,
                  muted_text.a);
    y_cursor += line_h + section_gap;
    draw_text_5x7_clipped(renderer,
                          &panel_clip,
                          panel.x + pad,
                          y_cursor,
                          root,
                          1,
                          primary_text.r,
                          primary_text.g,
                          primary_text.b,
                          primary_text.a);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  "ACTIVE",
                  1,
                  muted_text.r,
                  muted_text.g,
                  muted_text.b,
                  muted_text.a);
    y_cursor += line_h + section_gap;
    draw_text_5x7_clipped(renderer,
                          &panel_clip,
                          panel.x + pad,
                          y_cursor,
                          pack_path,
                          1,
                          primary_text.r,
                          primary_text.g,
                          primary_text.b,
                          primary_text.a);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  g_pack_panel_cache.status[0] ? g_pack_panel_cache.status : "scanning...",
                  1,
                  primary_text.r,
                  primary_text.g,
                  primary_text.b,
                  primary_text.a);
    y_cursor += line_h + section_gap;

    {
        list_box = (SDL_Rect){
            panel.x + pad,
            y_cursor,
            panel.w - (pad * 2),
            panel.y + panel.h - y_cursor - pad
        };
        kit_ui_sdl_fill_rounded_rect(renderer, &list_box, list_radius, hud_style.readout_fill);

        row_h = datalab_text_line_height(1) + datalab_scaled_px(2.0f);
        if (row_h < datalab_scaled_px(9.0f)) {
            row_h = datalab_scaled_px(9.0f);
        }
        max_rows = (list_box.h - datalab_scaled_px(6.0f)) / row_h;
        if (max_rows < 1) {
            max_rows = 1;
        }
        start_y = list_box.y + datalab_scaled_px(3.0f);
        if (g_pack_panel_cache.file_count > 0u) {
            size_t visible_rows = (size_t)max_rows;
            if (app_state->panel_selected_index >= visible_rows) {
                start_idx = app_state->panel_selected_index - visible_rows + 1u;
            }
            if (start_idx + visible_rows > g_pack_panel_cache.file_count) {
                if (g_pack_panel_cache.file_count > visible_rows) {
                    start_idx = g_pack_panel_cache.file_count - visible_rows;
                } else {
                    start_idx = 0u;
                }
            }
        }
        if (g_pack_panel_cache.file_count == 0u) {
            SDL_Rect list_clip = {
                list_box.x + datalab_scaled_px(2.0f),
                list_box.y + datalab_scaled_px(2.0f),
                list_box.w - datalab_scaled_px(4.0f),
                list_box.h - datalab_scaled_px(4.0f)
            };
            draw_text_5x7_clipped(renderer,
                                  &list_clip,
                                  list_box.x + datalab_scaled_px(6.0f),
                                  start_y,
                                  "NO SUPPORTED FILES (.PACK/.BMP) IN INPUT ROOT",
                                  1,
                                  error_text.r,
                                  error_text.g,
                                  error_text.b,
                                  error_text.a);
        } else {
            SDL_Rect list_clip = {
                list_box.x + datalab_scaled_px(2.0f),
                list_box.y + datalab_scaled_px(2.0f),
                list_box.w - datalab_scaled_px(4.0f),
                list_box.h - datalab_scaled_px(4.0f)
            };
            size_t i;
            for (i = 0u; (int)i < max_rows; ++i) {
                size_t idx = start_idx + i;
                int y = start_y + ((int)i * row_h);
                const char *name = NULL;
                int is_selected = 0;
                int is_active = 0;
                if (idx >= g_pack_panel_cache.file_count) {
                    break;
                }
                name = g_pack_panel_cache.files[idx];
                is_selected = (idx == app_state->panel_selected_index);
                is_active = (active_name[0] != '\0') && (strcasecmp(name, active_name) == 0);
                if (is_selected || is_active) {
                    SDL_Rect hi = {
                        list_box.x + datalab_scaled_px(2.0f),
                        y - datalab_scaled_px(1.0f),
                        list_box.w - datalab_scaled_px(4.0f),
                        row_h
                    };
                    KitRenderColor hi_fill = is_selected ? hud_style.button_active_fill : hud_style.button_fill;
                    row_radius = (int)kit_ui_corner_radius_clamp(kit_ui_corner_radius_for_inset((float)list_radius,
                                                                                               datalab_scaled_px(2.0f)),
                                                                 (float)hi.w,
                                                                 (float)hi.h);
                    kit_ui_sdl_fill_rounded_rect(renderer, &hi, row_radius, hi_fill);
                }
                draw_text_5x7_clipped(renderer,
                                      &list_clip,
                                      list_box.x + datalab_scaled_px(6.0f),
                                      y,
                                      name,
                                      1,
                                      (is_selected || is_active) ? primary_text.r : muted_text.r,
                                      (is_selected || is_active) ? primary_text.g : muted_text.g,
                                      (is_selected || is_active) ? primary_text.b : muted_text.b,
                                      (is_selected || is_active) ? primary_text.a : muted_text.a);
            }
        }
    }
}
