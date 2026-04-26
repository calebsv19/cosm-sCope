#include "render/render_view_internal.h"
#include "render/render_view_authoring_overlay_shared.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "kit_workspace_authoring_ui.h"


typedef struct DatalabAuthoringOverlayHost {
    SDL_Renderer *renderer;
    const DatalabFrame *frame;
    const DatalabAppState *app_state;
} DatalabAuthoringOverlayHost;

DatalabAuthoringOverlayUiState g_datalab_authoring_overlay_ui = {0};

int datalab_overlay_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static SDL_Rect datalab_overlay_rect_from_core(CorePaneRect rect) {
    SDL_Rect out = { 0, 0, 0, 0 };
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    return out;
}

static const char *datalab_overlay_profile_name(const DatalabFrame *frame) {
    if (!frame) {
        return "unknown";
    }
    switch (frame->profile) {
        case DATALAB_PROFILE_PHYSICS:
            return "physics";
        case DATALAB_PROFILE_DAW:
            return "daw";
        case DATALAB_PROFILE_TRACE:
            return "trace";
        case DATALAB_PROFILE_SKETCH:
            return "sketch";
        default:
            return "unknown";
    }
}

static int datalab_overlay_pane_mode_active(const DatalabAppState *app_state) {
    int layout_mode = DATALAB_AUTHORING_LAYOUT_MODE_RUNTIME;
    int overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;

    if (!app_state) {
        return 1;
    }
    if (app_state->workspace_authoring_stub_active) {
        layout_mode = DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING;
    }
    overlay_mode = (int)app_state->workspace_authoring_overlay_mode;
    return kit_workspace_authoring_ui_pane_overlay_visible(layout_mode,
                                                           DATALAB_AUTHORING_LAYOUT_MODE_AUTHORING,
                                                           overlay_mode,
                                                           DATALAB_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME);
}

static void datalab_overlay_capture_top_buttons(int viewport_width, int pane_overlay_active) {
    memset(g_datalab_authoring_overlay_ui.top_buttons, 0, sizeof(g_datalab_authoring_overlay_ui.top_buttons));
    g_datalab_authoring_overlay_ui.top_button_count =
        kit_workspace_authoring_ui_build_overlay_buttons(viewport_width,
                                                         1,
                                                         pane_overlay_active,
                                                         g_datalab_authoring_overlay_ui.top_buttons,
                                                         8u);
    g_datalab_authoring_overlay_ui.pane_overlay_active = pane_overlay_active;
}

static void datalab_overlay_draw_top_buttons(SDL_Renderer *renderer, const DatalabAuthoringThemePalette *palette) {
    uint32_t i;

    if (!renderer || !palette) {
        return;
    }

    for (i = 0u; i < g_datalab_authoring_overlay_ui.top_button_count; ++i) {
        const KitWorkspaceAuthoringOverlayButton *button = &g_datalab_authoring_overlay_ui.top_buttons[i];
        SDL_Rect rect;
        if (!button->visible) {
            continue;
        }
        rect = datalab_overlay_rect_from_core(button->rect);
        datalab_overlay_draw_button(renderer,
                                    &rect,
                                    button->label,
                                    g_datalab_authoring_overlay_ui.hover_top_button == button->id,
                                    0,
                                    palette);
    }
}

static void datalab_overlay_draw_pane_takeover(SDL_Renderer *renderer,
                                               const DatalabFrame *frame,
                                               const DatalabAppState *app_state,
                                               const DatalabAuthoringThemePalette *palette,
                                               int ww,
                                               int wh) {
    SDL_Rect pane = {0};
    SDL_Rect header = {0};
    char status_line[192];
    int pad = 0;

    if (!renderer || !app_state || !palette || ww <= 0 || wh <= 0) {
        return;
    }

    pad = datalab_scaled_px(12.0f);
    pane.x = 0;
    pane.y = datalab_scaled_px(34.0f);
    pane.w = ww;
    pane.h = wh - pane.y;
    if (pane.h < datalab_scaled_px(50.0f)) {
        pane.h = datalab_scaled_px(50.0f);
    }
    header = pane;
    header.h = datalab_scaled_px(24.0f);

    SDL_SetRenderDrawColor(renderer, palette->clear_r, palette->clear_g, palette->clear_b, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, palette->pane_fill_r, palette->pane_fill_g, palette->pane_fill_b, 255);
    SDL_RenderFillRect(renderer, &pane);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
    SDL_RenderDrawRect(renderer, &pane);
    SDL_SetRenderDrawColor(renderer, palette->shell_fill_r, palette->shell_fill_g, palette->shell_fill_b, 255);
    SDL_RenderFillRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, palette->shell_border_r, palette->shell_border_g, palette->shell_border_b, 255);
    SDL_RenderDrawRect(renderer, &header);

    draw_text_5x7(renderer,
                  header.x + pad,
                  header.y + datalab_scaled_px(6.0f),
                  "Pane 1 [DataLab Host]",
                  1,
                  palette->text_primary_r,
                  palette->text_primary_g,
                  palette->text_primary_b,
                  255);
    snprintf(status_line,
             sizeof(status_line),
             "profile:%s pending:%u apply:%u cancel:%u theme:%s",
             datalab_overlay_profile_name(frame),
             (unsigned int)app_state->workspace_authoring_pending_stub,
             (unsigned int)app_state->workspace_authoring_apply_count,
             (unsigned int)app_state->workspace_authoring_cancel_count,
             datalab_overlay_theme_name(datalab_overlay_selected_theme(app_state)));
    draw_text_5x7(renderer,
                  pane.x + pad,
                  pane.y + datalab_scaled_px(38.0f),
                  status_line,
                  1,
                  palette->text_secondary_r,
                  palette->text_secondary_g,
                  palette->text_secondary_b,
                  255);
    datalab_overlay_draw_centered_text(renderer,
                                       &pane,
                                       pane.y + (pane.h / 2),
                                       "DataLab Authoring Overlay (Pane)",
                                       palette->text_primary_r,
                                       palette->text_primary_g,
                                       palette->text_primary_b,
                                       255);
    g_datalab_authoring_overlay_ui.font_controls_valid = 0u;
    g_datalab_authoring_overlay_ui.hover_font_hit = DATALAB_AUTHORING_FONT_HIT_NONE;
}

static CoreResult datalab_workspace_authoring_draw_scene_callback(
    void *host_context,
    const KitWorkspaceAuthoringRenderDeriveFrame *derive) {
    DatalabAuthoringOverlayHost *host = (DatalabAuthoringOverlayHost *)host_context;
    DatalabAuthoringThemePalette palette = {0};
    DatalabWorkspaceAuthoringThemePreset selected_theme = DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    int pane_overlay_active = 1;
    SDL_Rect help_strip = {0};

    if (!host || !host->renderer || !host->frame || !host->app_state || !derive) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring overlay draw request" };
    }

    selected_theme = datalab_overlay_selected_theme(host->app_state);
    datalab_overlay_theme_palette(selected_theme,
                                  &host->app_state->workspace_authoring_custom_theme,
                                  &palette);

    pane_overlay_active = datalab_overlay_pane_mode_active(host->app_state);
    g_datalab_authoring_overlay_ui.viewport_w = derive->width;
    g_datalab_authoring_overlay_ui.viewport_h = derive->height;
    datalab_overlay_capture_top_buttons(derive->width, pane_overlay_active);

    if (pane_overlay_active) {
        datalab_overlay_draw_pane_takeover(host->renderer,
                                           host->frame,
                                           host->app_state,
                                           &palette,
                                           derive->width,
                                           derive->height);
    } else {
        datalab_overlay_draw_font_theme_takeover(host->renderer,
                                                 host->app_state,
                                                 &palette,
                                                 derive->width,
                                                 derive->height);
    }

    datalab_overlay_draw_top_buttons(host->renderer, &palette);

    help_strip.x = datalab_scaled_px(8.0f);
    help_strip.y = datalab_scaled_px(30.0f);
    help_strip.w = datalab_overlay_clamp_int(derive->width - datalab_scaled_px(16.0f), 20, derive->width);
    help_strip.h = datalab_scaled_px(18.0f);
    SDL_SetRenderDrawBlendMode(host->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(host->renderer, palette.clear_r, palette.clear_g, palette.clear_b, 210);
    SDL_RenderFillRect(host->renderer, &help_strip);
    SDL_SetRenderDrawColor(host->renderer,
                           palette.shell_border_r,
                           palette.shell_border_g,
                           palette.shell_border_b,
                           238);
    SDL_RenderDrawRect(host->renderer, &help_strip);
    draw_text_5x7(host->renderer,
                  help_strip.x + datalab_scaled_px(6.0f),
                  help_strip.y + datalab_scaled_px(4.0f),
                  "ALT+C+V toggle | TAB overlay | ENTER apply | ESC cancel | mouse: theme/text controls",
                  1,
                  palette.text_secondary_r,
                  palette.text_secondary_g,
                  palette.text_secondary_b,
                  255);
    return core_result_ok();
}

void datalab_workspace_authoring_submit_takeover(SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 const DatalabFrame *frame,
                                                 const DatalabAppState *app_state,
                                                 const char *title,
                                                 DatalabRenderSubmitOutcome *outcome) {
    DatalabAuthoringOverlayHost host = {0};
    KitWorkspaceAuthoringRenderDeriveFrame shared_derive = {0};
    KitWorkspaceAuthoringRenderSubmitOutcome shared_outcome = {0};
    int ww = 0;
    int wh = 0;

    if (!outcome) {
        return;
    }
    memset(outcome, 0, sizeof(*outcome));
    if (!window || !renderer || !frame || !app_state) {
        outcome->result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring takeover request" };
        return;
    }

    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    if (ww <= 0 || wh <= 0) {
        outcome->result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab authoring viewport" };
        return;
    }

    kit_workspace_authoring_ui_derive_frame(&shared_derive,
                                            ww,
                                            wh,
                                            1,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0.0f,
                                            0.0f,
                                            0.0f,
                                            0u,
                                            0u,
                                            0u,
                                            (int)(frame->frame_index & 0x7fffffffULL),
                                            (int)app_state->workspace_authoring_pending_stub);

    host.renderer = renderer;
    host.frame = frame;
    host.app_state = app_state;

    kit_workspace_authoring_ui_submit_frame(&host,
                                            &shared_derive,
                                            datalab_workspace_authoring_draw_scene_callback,
                                            NULL,
                                            NULL,
                                            &shared_outcome);
    outcome->result = shared_outcome.draw_result;
    if (outcome->result.code != CORE_OK) {
        return;
    }

    SDL_SetWindowTitle(window, (title && title[0]) ? title : "DataLab");
    SDL_RenderPresent(renderer);
    outcome->presented = 1u;
}
