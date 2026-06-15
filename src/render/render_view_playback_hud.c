#include "render/render_view_internal.h"

#include <stdio.h>
#include <string.h>

#include "kit_ui_sdl.h"
#include "render/render_view_authoring_overlay_shared.h"

typedef enum DatalabPlaybackHudAction {
    DATALAB_PLAYBACK_HUD_ACTION_NONE = 0,
    DATALAB_PLAYBACK_HUD_ACTION_PREV,
    DATALAB_PLAYBACK_HUD_ACTION_PLAY_PAUSE,
    DATALAB_PLAYBACK_HUD_ACTION_NEXT,
    DATALAB_PLAYBACK_HUD_ACTION_SPEED_DOWN,
    DATALAB_PLAYBACK_HUD_ACTION_SPEED_UP,
    DATALAB_PLAYBACK_HUD_ACTION_MODE_LOOP,
    DATALAB_PLAYBACK_HUD_ACTION_MODE_BOUNCE
} DatalabPlaybackHudAction;

typedef struct DatalabPlaybackHudButton {
    SDL_Rect rect;
    DatalabPlaybackHudAction action;
    int enabled;
} DatalabPlaybackHudButton;

typedef struct DatalabPlaybackHudUiState {
    SDL_Rect panel_rect;
    DatalabPlaybackHudButton buttons[8];
    size_t button_count;
} DatalabPlaybackHudUiState;

static DatalabPlaybackHudUiState g_playback_hud_ui;

static int datalab_hud_point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) {
        return 0;
    }
    return x >= rect->x && y >= rect->y &&
           x < rect->x + rect->w && y < rect->y + rect->h;
}

static int datalab_hud_map_window_to_renderer_point(SDL_Window *window,
                                                    SDL_Renderer *renderer,
                                                    int window_x,
                                                    int window_y,
                                                    int *out_render_x,
                                                    int *out_render_y) {
    int window_w = 0;
    int window_h = 0;
    int render_w = 0;
    int render_h = 0;
    if (!window || !renderer || !out_render_x || !out_render_y) {
        return 0;
    }
    SDL_GetWindowSize(window, &window_w, &window_h);
    SDL_GetRendererOutputSize(renderer, &render_w, &render_h);
    if (window_w <= 0 || window_h <= 0 || render_w <= 0 || render_h <= 0) {
        return 0;
    }
    *out_render_x = (window_x * render_w) / window_w;
    *out_render_y = (window_y * render_h) / window_h;
    return 1;
}

static const char *datalab_hud_speed_label(int speed_index) {
    static const char *k_labels[] = {"0.25x", "0.5x", "1x", "2x", "4x"};
    speed_index = datalab_playback_speed_index_clamp(speed_index);
    return k_labels[speed_index];
}

static void datalab_hud_add_button(SDL_Rect rect,
                                   DatalabPlaybackHudAction action,
                                   int enabled) {
    if (g_playback_hud_ui.button_count >=
        sizeof(g_playback_hud_ui.buttons) / sizeof(g_playback_hud_ui.buttons[0])) {
        return;
    }
    g_playback_hud_ui.buttons[g_playback_hud_ui.button_count].rect = rect;
    g_playback_hud_ui.buttons[g_playback_hud_ui.button_count].action = action;
    g_playback_hud_ui.buttons[g_playback_hud_ui.button_count].enabled = enabled;
    g_playback_hud_ui.button_count++;
}

static void datalab_hud_apply_action(DatalabAppState *app_state,
                                     DatalabPlaybackHudAction action) {
    uint32_t now = SDL_GetTicks();
    if (!app_state) {
        return;
    }
    switch (action) {
        case DATALAB_PLAYBACK_HUD_ACTION_PREV:
            app_state->panel_selection_delta -= 1;
            app_state->panel_open_selected_requested = 1;
            break;
        case DATALAB_PLAYBACK_HUD_ACTION_PLAY_PAUSE:
            app_state->playback_active = !app_state->playback_active;
            if (app_state->playback_interval_ms == 0u) {
                app_state->playback_interval_ms =
                    datalab_playback_interval_for_speed_index(app_state->playback_speed_index);
            }
            app_state->playback_last_advance_ticks = now;
            break;
        case DATALAB_PLAYBACK_HUD_ACTION_NEXT:
            app_state->panel_selection_delta += 1;
            app_state->panel_open_selected_requested = 1;
            break;
        case DATALAB_PLAYBACK_HUD_ACTION_SPEED_DOWN:
            app_state->playback_speed_index =
                datalab_playback_speed_index_clamp(app_state->playback_speed_index - 1);
            app_state->playback_interval_ms =
                datalab_playback_interval_for_speed_index(app_state->playback_speed_index);
            app_state->playback_last_advance_ticks = now;
            break;
        case DATALAB_PLAYBACK_HUD_ACTION_SPEED_UP:
            app_state->playback_speed_index =
                datalab_playback_speed_index_clamp(app_state->playback_speed_index + 1);
            app_state->playback_interval_ms =
                datalab_playback_interval_for_speed_index(app_state->playback_speed_index);
            app_state->playback_last_advance_ticks = now;
            break;
        case DATALAB_PLAYBACK_HUD_ACTION_MODE_LOOP:
            app_state->playback_mode = DATALAB_PLAYBACK_MODE_LOOP;
            app_state->playback_direction = 1;
            break;
        case DATALAB_PLAYBACK_HUD_ACTION_MODE_BOUNCE:
            app_state->playback_mode = DATALAB_PLAYBACK_MODE_BOUNCE;
            app_state->playback_direction = 1;
            break;
        case DATALAB_PLAYBACK_HUD_ACTION_NONE:
        default:
            break;
    }
}

static int datalab_hud_text_measure(void *user,
                                    const char *text,
                                    int scale,
                                    int *out_width,
                                    int *out_height) {
    (void)user;
    return datalab_measure_text(scale, text, out_width, out_height);
}

static int datalab_hud_text_line_height(void *user, int scale) {
    (void)user;
    return datalab_text_line_height(scale);
}

static void datalab_hud_draw_text_clipped(void *user,
                                          SDL_Renderer *renderer,
                                          const SDL_Rect *clip_rect,
                                          int x,
                                          int y,
                                          const char *text,
                                          int scale,
                                          KitRenderColor color) {
    (void)user;
    draw_text_5x7_clipped(renderer,
                          clip_rect,
                          x,
                          y,
                          text,
                          scale,
                          color.r,
                          color.g,
                          color.b,
                          color.a);
}

int datalab_playback_hud_route_mouse_event(SDL_Window *window,
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
    if (!datalab_hud_map_window_to_renderer_point(window,
                                                  renderer,
                                                  event->button.x,
                                                  event->button.y,
                                                  &pointer_x,
                                                  &pointer_y)) {
        return 0;
    }
    if (!datalab_hud_point_in_rect(&g_playback_hud_ui.panel_rect, pointer_x, pointer_y)) {
        return 0;
    }
    for (i = 0u; i < g_playback_hud_ui.button_count; ++i) {
        if (datalab_hud_point_in_rect(&g_playback_hud_ui.buttons[i].rect, pointer_x, pointer_y)) {
            if (g_playback_hud_ui.buttons[i].enabled) {
                datalab_hud_apply_action(app_state, g_playback_hud_ui.buttons[i].action);
            }
            return 1;
        }
    }
    return 1;
}

void datalab_draw_playback_hud(SDL_Renderer *renderer, const DatalabAppState *app_state) {
    SDL_Rect button;
    SDL_Rect panel;
    SDL_Rect readout_rect;
    char readout[256];
    char position[32];
    const char *selected_name = "";
    const char *play_label = NULL;
    const char *labels[7];
    int enabled[7];
    int selected[7];
    DatalabPlaybackHudAction actions[7];
    int ww = 0;
    int wh = 0;
    int pad = 0;
    int gap = 0;
    int button_h = 0;
    int small_w = 0;
    int play_w = 0;
    int speed_w = 0;
    int mode_w = 0;
    int file_count = 0;
    int selected_index = 0;
    int has_files = 0;
    int can_slow = 0;
    int can_fast = 0;
    size_t i = 0u;
    DatalabAuthoringThemePalette palette = {0};
    DatalabWorkspaceAuthoringThemePreset preset = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    KitUiHudStyle style;
    KitUiHudButtonRowConfig row_config;
    KitUiHudButtonRowLayout row_layout;
    KitUiSdlTextApi text_api;

    if (!renderer || !app_state || app_state->workspace_authoring_stub_active) {
        memset(&g_playback_hud_ui, 0, sizeof(g_playback_hud_ui));
        return;
    }
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    if (ww <= 0 || wh <= 0) {
        memset(&g_playback_hud_ui, 0, sizeof(g_playback_hud_ui));
        return;
    }

    memset(&g_playback_hud_ui, 0, sizeof(g_playback_hud_ui));
    pad = datalab_scaled_px(8.0f);
    gap = datalab_scaled_px(5.0f);
    button_h = datalab_scaled_px(28.0f);
    small_w = datalab_scaled_px(38.0f);
    play_w = datalab_scaled_px(58.0f);
    speed_w = datalab_scaled_px(54.0f);
    mode_w = datalab_scaled_px(62.0f);
    if (ww < datalab_scaled_px(560.0f)) {
        small_w = datalab_scaled_px(32.0f);
        play_w = datalab_scaled_px(50.0f);
        speed_w = datalab_scaled_px(46.0f);
        mode_w = datalab_scaled_px(54.0f);
        gap = datalab_scaled_px(4.0f);
    }

    file_count = (int)datalab_session_controls_file_count();
    has_files = file_count > 0;
    selected_index = file_count > 0 ? (int)app_state->panel_selected_index + 1 : 0;
    can_slow = app_state->playback_speed_index > DATALAB_PLAYBACK_SPEED_INDEX_MIN;
    can_fast = app_state->playback_speed_index < DATALAB_PLAYBACK_SPEED_INDEX_MAX;
    selected_name = datalab_session_controls_selected_file_name(app_state);
    snprintf(position, sizeof(position), "%d/%d", selected_index, file_count);
    snprintf(readout,
             sizeof(readout),
             "%s  %s  %s",
             position,
             datalab_hud_speed_label(app_state->playback_speed_index),
             selected_name && selected_name[0] ? selected_name : "no file");

    play_label = app_state->playback_active ? "Pause" : "Play";
    labels[0] = "<";
    labels[1] = play_label;
    labels[2] = ">";
    labels[3] = "Slow";
    labels[4] = "Fast";
    labels[5] = "Loop";
    labels[6] = "Bounce";
    actions[0] = DATALAB_PLAYBACK_HUD_ACTION_PREV;
    actions[1] = DATALAB_PLAYBACK_HUD_ACTION_PLAY_PAUSE;
    actions[2] = DATALAB_PLAYBACK_HUD_ACTION_NEXT;
    actions[3] = DATALAB_PLAYBACK_HUD_ACTION_SPEED_DOWN;
    actions[4] = DATALAB_PLAYBACK_HUD_ACTION_SPEED_UP;
    actions[5] = DATALAB_PLAYBACK_HUD_ACTION_MODE_LOOP;
    actions[6] = DATALAB_PLAYBACK_HUD_ACTION_MODE_BOUNCE;
    enabled[0] = has_files;
    enabled[1] = has_files;
    enabled[2] = has_files;
    enabled[3] = can_slow;
    enabled[4] = can_fast;
    enabled[5] = 1;
    enabled[6] = 1;
    selected[0] = 0;
    selected[1] = app_state->playback_active;
    selected[2] = 0;
    selected[3] = 0;
    selected[4] = 0;
    selected[5] = app_state->playback_mode == DATALAB_PLAYBACK_MODE_LOOP;
    selected[6] = app_state->playback_mode == DATALAB_PLAYBACK_MODE_BOUNCE;

    kit_ui_hud_button_row_config_init(&row_config);
    row_config.viewport_width = (float)ww;
    row_config.viewport_height = (float)wh;
    row_config.max_width = (float)(ww - datalab_scaled_px(20.0f));
    row_config.pad = (float)pad;
    row_config.gap = (float)gap;
    row_config.button_height = (float)button_h;
    row_config.bottom_margin = (float)datalab_scaled_px(12.0f);
    row_config.min_top_y = (float)datalab_scaled_px(80.0f);
    row_config.readout_min_width = (float)datalab_scaled_px(80.0f);
    row_config.readout_max_width = (float)datalab_scaled_px(260.0f);
    row_config.button_count = 7u;
    row_config.button_widths[0] = (float)small_w;
    row_config.button_widths[1] = (float)play_w;
    row_config.button_widths[2] = (float)small_w;
    row_config.button_widths[3] = (float)speed_w;
    row_config.button_widths[4] = (float)speed_w;
    row_config.button_widths[5] = (float)mode_w;
    row_config.button_widths[6] = (float)mode_w;

    preset = datalab_overlay_selected_theme(app_state);
    datalab_overlay_theme_palette(preset, &app_state->workspace_authoring_custom_theme, &palette);
    datalab_overlay_hud_style_from_palette(&palette, &style);
    style.panel_corner_radius = (float)datalab_scaled_px(style.panel_corner_radius);
    style.button_corner_radius = kit_ui_hud_button_row_control_corner_radius(&style, &row_config);

    if (!kit_ui_hud_button_row_layout(&row_config, &row_layout)) {
        memset(&g_playback_hud_ui, 0, sizeof(g_playback_hud_ui));
        return;
    }

    panel = kit_ui_sdl_rect_from_render(row_layout.panel_rect);
    g_playback_hud_ui.panel_rect = panel;

    text_api = (KitUiSdlTextApi){
        0,
        1,
        datalab_scaled_px(4.0f),
        datalab_hud_text_measure,
        datalab_hud_text_line_height,
        datalab_hud_draw_text_clipped
    };
    kit_ui_sdl_fill_rounded_rect(renderer,
                                 &panel,
                                 (int)style.panel_corner_radius,
                                 style.panel_fill);

    for (i = 0u; i < row_layout.button_count; ++i) {
        KitUiButtonState button_state;
        kit_ui_button_state_init(&button_state);
        button_state.selected = selected[i];
        button_state.disabled = !enabled[i];
        button = kit_ui_sdl_rect_from_render(row_layout.button_rects[i]);
        kit_ui_sdl_draw_button(renderer, &button, labels[i], &button_state, &style, &text_api);
        datalab_hud_add_button(button, actions[i], enabled[i]);
    }

    if (row_layout.has_readout) {
        readout_rect = kit_ui_sdl_rect_from_render(row_layout.readout_rect);
        text_api.clip_padding_x = datalab_scaled_px(8.0f);
        kit_ui_sdl_draw_readout(renderer, &readout_rect, readout, &style, &text_api);
    }
}
