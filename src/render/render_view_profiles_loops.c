#include "render/render_view_internal.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ui/input.h"

#define DATALAB_PANEL_MAX_FILES 160
#define DATALAB_PANEL_REFRESH_MS 1200u

typedef struct DatalabPackPanelCache {
    char scanned_root[DATALAB_APP_PATH_CAP];
    char files[DATALAB_PANEL_MAX_FILES][DATALAB_APP_PATH_CAP];
    size_t file_count;
    uint32_t last_scan_ticks;
    char status[160];
} DatalabPackPanelCache;

static DatalabPackPanelCache g_pack_panel_cache;

static int datalab_pack_ext(const char *name) {
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

static int datalab_name_ci_cmp(const void *a, const void *b) {
    const char *aa = (const char *)a;
    const char *bb = (const char *)b;
    return strcasecmp(aa, bb);
}

static int datalab_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static const char *datalab_path_basename(const char *path) {
    const char *base = NULL;
    if (!path || path[0] == '\0') {
        return "";
    }
    base = strrchr(path, '/');
    if (!base) {
        base = strrchr(path, '\\');
    }
    return base ? (base + 1) : path;
}

static void datalab_panel_rescan(const char *root, DatalabPackPanelCache *cache) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!root || !cache) {
        return;
    }
    cache->file_count = 0u;
    cache->status[0] = '\0';
    snprintf(cache->scanned_root, sizeof(cache->scanned_root), "%s", root);
    dir = opendir(root);
    if (!dir) {
        snprintf(cache->status, sizeof(cache->status), "input root unavailable (press O to reselect)");
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!datalab_pack_ext(entry->d_name)) {
            continue;
        }
        if (cache->file_count >= DATALAB_PANEL_MAX_FILES) {
            break;
        }
        snprintf(cache->files[cache->file_count],
                 sizeof(cache->files[cache->file_count]),
                 "%s",
                 entry->d_name);
        cache->file_count++;
    }
    closedir(dir);
    if (cache->file_count > 1u) {
        qsort(cache->files, cache->file_count, sizeof(cache->files[0]), datalab_name_ci_cmp);
    }
    if (cache->file_count == 0u) {
        snprintf(cache->status, sizeof(cache->status), "found 0 .pack files");
    } else {
        snprintf(cache->status, sizeof(cache->status), "found %zu .pack files", cache->file_count);
    }
}

static void datalab_panel_tick(DatalabAppState *app_state) {
    const char *root = NULL;
    uint32_t now_ticks = 0u;
    if (!app_state) {
        return;
    }
    if (app_state->input_root[0] == '\0') {
        g_pack_panel_cache.file_count = 0u;
        g_pack_panel_cache.scanned_root[0] = '\0';
        g_pack_panel_cache.last_scan_ticks = 0u;
        snprintf(g_pack_panel_cache.status, sizeof(g_pack_panel_cache.status), "no input root selected (press O)");
        app_state->panel_selected_index = 0u;
        app_state->panel_selection_delta = 0;
        app_state->panel_open_selected_requested = 0;
        return;
    }
    root = app_state->input_root;
    now_ticks = SDL_GetTicks();
    if (app_state->panel_rescan_requested ||
        strncmp(g_pack_panel_cache.scanned_root, root, sizeof(g_pack_panel_cache.scanned_root)) != 0 ||
        (now_ticks - g_pack_panel_cache.last_scan_ticks) > DATALAB_PANEL_REFRESH_MS) {
        datalab_panel_rescan(root, &g_pack_panel_cache);
        g_pack_panel_cache.last_scan_ticks = now_ticks;
        app_state->panel_rescan_requested = 0;
    }

    if (g_pack_panel_cache.file_count == 0u) {
        app_state->panel_selected_index = 0u;
        app_state->panel_selection_delta = 0;
    } else {
        int delta = app_state->panel_selection_delta;
        if (delta != 0) {
            long idx = (long)app_state->panel_selected_index + (long)delta;
            if (idx < 0) {
                idx = 0;
            }
            if ((size_t)idx >= g_pack_panel_cache.file_count) {
                idx = (long)(g_pack_panel_cache.file_count - 1u);
            }
            app_state->panel_selected_index = (size_t)idx;
            app_state->panel_selection_delta = 0;
        } else if (app_state->panel_selected_index >= g_pack_panel_cache.file_count) {
            app_state->panel_selected_index = g_pack_panel_cache.file_count - 1u;
        }
    }

    if (app_state->panel_open_selected_requested) {
        app_state->panel_open_selected_requested = 0;
        app_state->panel_requested_pack_path[0] = '\0';
        if (g_pack_panel_cache.file_count > 0u && app_state->panel_selected_index < g_pack_panel_cache.file_count) {
            snprintf(app_state->panel_requested_pack_path,
                     sizeof(app_state->panel_requested_pack_path),
                     "%s/%s",
                     root,
                     g_pack_panel_cache.files[app_state->panel_selected_index]);
        } else {
            snprintf(g_pack_panel_cache.status, sizeof(g_pack_panel_cache.status), "no .pack file selected");
        }
    }
}

void datalab_draw_session_controls(SDL_Renderer *renderer, const DatalabAppState *app_state) {
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
    size_t start_idx = 0u;
    if (!renderer || !app_state) {
        return;
    }

    root = (app_state->input_root[0] != '\0') ? app_state->input_root : "<not set>";
    pack_path = app_state->pack_path && app_state->pack_path[0] ? app_state->pack_path : "<none>";
    active_name = datalab_path_basename(pack_path);
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
    pad = datalab_scaled_px(8.0f);
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
    (void)datalab_measure_text(1, "O picker | U/J nav | Enter load | F5 rescan", &measured_w, &measured_h);
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
    (void)datalab_measure_text(1, "NO .PACK FILES IN INPUT ROOT", &measured_w, &measured_h);
    if (measured_w > content_w) content_w = measured_w;

    panel.x = datalab_scaled_px(10.0f);
    panel.y = datalab_scaled_px(10.0f);
    panel.w = datalab_clamp_int(content_w + (pad * 2) + datalab_scaled_px(8.0f), min_panel_w, max_panel_w);
    panel.h = datalab_clamp_int((line_h * 7) + (section_gap * 6) + datalab_scaled_px(120.0f), min_panel_h, max_panel_h);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 12, 18, 190);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 100, 120, 210);
    SDL_RenderDrawRect(renderer, &panel);
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
                  220,
                  230,
                  240,
                  255);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  "O picker | U/J nav | Enter load | F5 rescan",
                  1,
                  210,
                  220,
                  230,
                  255);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  "ROOT",
                  1,
                  180,
                  190,
                  205,
                  255);
    y_cursor += line_h + section_gap;
    draw_text_5x7_clipped(renderer,
                          &panel_clip,
                          panel.x + pad,
                          y_cursor,
                          root,
                          1,
                          170,
                          190,
                          210,
                          255);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  "ACTIVE",
                  1,
                  180,
                  190,
                  205,
                  255);
    y_cursor += line_h + section_gap;
    draw_text_5x7_clipped(renderer,
                          &panel_clip,
                          panel.x + pad,
                          y_cursor,
                          pack_path,
                          1,
                          170,
                          190,
                          210,
                          255);
    y_cursor += line_h + section_gap;
    draw_text_5x7(renderer,
                  panel.x + pad,
                  y_cursor,
                  g_pack_panel_cache.status[0] ? g_pack_panel_cache.status : "scanning...",
                  1,
                  150,
                  215,
                  160,
                  255);
    y_cursor += line_h + section_gap;

    {
        list_box = (SDL_Rect){
            panel.x + pad,
            y_cursor,
            panel.w - (pad * 2),
            panel.y + panel.h - y_cursor - pad
        };
        SDL_SetRenderDrawColor(renderer, 18, 21, 30, 210);
        SDL_RenderFillRect(renderer, &list_box);
        SDL_SetRenderDrawColor(renderer, 70, 80, 100, 210);
        SDL_RenderDrawRect(renderer, &list_box);

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
                                  "NO .PACK FILES IN INPUT ROOT",
                                  1,
                                  230,
                                  150,
                                  140,
                                  255);
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
                    SDL_SetRenderDrawColor(renderer, is_selected ? 58 : 40, is_selected ? 86 : 66, 92, 220);
                    SDL_RenderFillRect(renderer, &hi);
                    SDL_SetRenderDrawColor(renderer, is_selected ? 118 : 88, is_selected ? 168 : 138, 188, 255);
                    SDL_RenderDrawRect(renderer, &hi);
                }
                draw_text_5x7_clipped(renderer,
                                      &list_clip,
                                      list_box.x + datalab_scaled_px(6.0f),
                                      y,
                                      name,
                                      1,
                                      (is_selected || is_active) ? 235 : 210,
                                      (is_selected || is_active) ? 245 : 220,
                                      (is_selected || is_active) ? 255 : 235,
                                      255);
            }
        }
    }
}

static double datalab_loop_elapsed_sec(Uint64 begin_counter,
                                       Uint64 end_counter,
                                       Uint64 perf_freq) {
    if (perf_freq == 0u || end_counter <= begin_counter) {
        return 0.0;
    }
    return (double)(end_counter - begin_counter) / (double)perf_freq;
}

enum {
    DATALAB_LOOP_RENDER_HEARTBEAT_MS = 250u,
    DATALAB_LOOP_RENDER_REASON_FORCE = 1u << 0,
    DATALAB_LOOP_RENDER_REASON_INPUT_INVALIDATE = 1u << 1,
    DATALAB_LOOP_RENDER_REASON_ASYNC_PANEL_RESCAN = 1u << 2,
    DATALAB_LOOP_RENDER_REASON_ASYNC_AUTHORING = 1u << 3,
    DATALAB_LOOP_RENDER_REASON_RESIZE = 1u << 4,
    DATALAB_LOOP_RENDER_REASON_HEARTBEAT = 1u << 5
};

typedef struct DatalabLoopBoundarySignals {
    uint8_t sync_input_invalidated;
    uint8_t async_panel_rescan_pending;
    uint8_t async_authoring_pending;
} DatalabLoopBoundarySignals;

typedef struct DatalabLoopFramePhases {
    DatalabInputFrame input_frame;
    DatalabLoopBoundarySignals boundary_signals;
    int panel_rescan_pending;
    int resize_pending;
    int wait_timeout_ms;
    uint32_t wait_blocked_ms;
    uint32_t wait_call_count;
    Uint64 frame_begin_counter;
    double frame_elapsed_sec;
    uint32_t render_reason_bits;
    uint8_t should_render;
} DatalabLoopFramePhases;

typedef struct DatalabLoopRunState {
    int quit;
    Uint64 perf_freq;
    uint32_t last_present_ticks;
    DatalabLoopWaitPolicyInput wait_policy_input;
    DatalabInputDiagTotals ir1_diag_totals;
    DatalabRenderDiagTotals rs1_diag_totals;
} DatalabLoopRunState;

typedef CoreResult (*DatalabLoopRenderStepFn)(SDL_Window *window,
                                              SDL_Renderer *renderer,
                                              const DatalabFrame *frame,
                                              DatalabAppState *app_state,
                                              void *lane_ctx,
                                              DatalabRenderSubmitOutcome *out_submit);

typedef struct DatalabLoopProfileOps {
    const char *lane_tag;
    void *lane_ctx;
    DatalabLoopRenderStepFn render_step;
} DatalabLoopProfileOps;

typedef struct DatalabPhysicsLoopContext {
    SDL_Texture *texture;
    uint8_t *density_rgba;
    uint8_t *speed_rgba;
    KitVizVecSegment *segments;
    size_t sample_count;
} DatalabPhysicsLoopContext;

typedef struct DatalabSketchLoopContext {
    SDL_Texture *texture;
} DatalabSketchLoopContext;

static void datalab_loop_handle_event(const SDL_Event *event,
                                      DatalabInputFrame *input_frame,
                                      DatalabAppState *app_state,
                                      int *quit,
                                      int *resize_pending) {
    DatalabWorkspaceAuthoringAdapterResult authoring_route = {0};
    if (!event || !input_frame || !app_state || !quit || !resize_pending) {
        return;
    }
    datalab_input_apply_event(input_frame, event);
    if (event->type == SDL_QUIT) {
        *quit = 1;
    } else if (event->type == SDL_WINDOWEVENT) {
        *resize_pending = 1;
    }
    if (datalab_workspace_authoring_route_mouse_event(event, app_state)) {
        return;
    }
    if (event->type == SDL_KEYDOWN) {
        datalab_workspace_authoring_route_keydown(&event->key, app_state, &authoring_route);
        if (!authoring_route.consumed) {
            datalab_handle_keydown(&event->key, app_state, quit);
        }
    }
}

static void datalab_loop_input_wait_and_drain(DatalabInputFrame *input_frame,
                                              DatalabAppState *app_state,
                                              int *quit,
                                              int wait_timeout_ms,
                                              uint32_t *out_wait_blocked_ms,
                                              uint32_t *out_wait_call_count,
                                              int *out_resize_pending) {
    SDL_Event event;
    if (!input_frame || !app_state || !quit || !out_wait_blocked_ms || !out_wait_call_count || !out_resize_pending) {
        return;
    }
    if (wait_timeout_ms > 0) {
        uint32_t wait_start = SDL_GetTicks();
        if (SDL_WaitEventTimeout(&event, wait_timeout_ms) == 1) {
            datalab_loop_handle_event(&event, input_frame, app_state, quit, out_resize_pending);
        }
        *out_wait_blocked_ms += (SDL_GetTicks() - wait_start);
        *out_wait_call_count += 1u;
    }
    while (SDL_PollEvent(&event)) {
        datalab_loop_handle_event(&event, input_frame, app_state, quit, out_resize_pending);
    }
}

static void datalab_loop_update_wait_policy(DatalabLoopWaitPolicyInput *policy,
                                            const DatalabInputFrame *input_frame,
                                            const DatalabAppState *app_state,
                                            int panel_rescan_pending,
                                            int resize_pending) {
    if (!policy || !input_frame || !app_state) {
        return;
    }
    policy->high_intensity_mode = 0u;
    policy->interaction_active = (input_frame->raw.sdl_event_count > 0u || app_state->workspace_authoring_pending_stub)
                                     ? 1u
                                     : 0u;
    policy->background_busy = panel_rescan_pending ? 1u : 0u;
    policy->resize_pending = resize_pending ? 1u : 0u;
}

static void datalab_loop_note_input_diag(const char *lane_tag,
                                         DatalabInputDiagTotals *totals,
                                         const DatalabInputFrame *input_frame) {
    if (!totals || !input_frame) {
        return;
    }
    totals->frame_count += 1u;
    totals->event_count_total += input_frame->raw.sdl_event_count;
    totals->routed_global_total += input_frame->route.routed_global_count;
    totals->routed_fallback_total += input_frame->route.routed_fallback_count;
    totals->invalidation_reason_bits_total += input_frame->invalidation.invalidation_reason_bits;
    if (datalab_ir1_diag_enabled()) {
        printf("[ir1] datalab-%s frame=%llu events=%u route(global=%u fallback=%u target=%d) "
               "invalidate(bits=0x%x target=%u full=%u) totals(frames=%llu events=%llu global=%llu fallback=%llu invalid_bits_sum=%llu)\n",
               lane_tag ? lane_tag : "unknown",
               (unsigned long long)totals->frame_count,
               (unsigned int)input_frame->raw.sdl_event_count,
               (unsigned int)input_frame->route.routed_global_count,
               (unsigned int)input_frame->route.routed_fallback_count,
               (int)input_frame->route.target_policy,
               (unsigned int)input_frame->invalidation.invalidation_reason_bits,
               (unsigned int)input_frame->invalidation.target_invalidation_count,
               (unsigned int)input_frame->invalidation.full_invalidation_count,
               (unsigned long long)totals->frame_count,
               (unsigned long long)totals->event_count_total,
               (unsigned long long)totals->routed_global_total,
               (unsigned long long)totals->routed_fallback_total,
               (unsigned long long)totals->invalidation_reason_bits_total);
    }
}

static void datalab_loop_frame_phase_wait_and_input(DatalabLoopFramePhases *phase,
                                                    DatalabLoopRunState *run_state,
                                                    DatalabAppState *app_state) {
    if (!phase || !run_state || !app_state) {
        return;
    }
    memset(phase, 0, sizeof(*phase));
    phase->frame_begin_counter = SDL_GetPerformanceCounter();
    phase->wait_timeout_ms = datalab_loop_compute_wait_timeout_ms(&run_state->wait_policy_input);
    datalab_input_frame_begin(&phase->input_frame);
    datalab_loop_input_wait_and_drain(&phase->input_frame,
                                      app_state,
                                      &run_state->quit,
                                      phase->wait_timeout_ms,
                                      &phase->wait_blocked_ms,
                                      &phase->wait_call_count,
                                      &phase->resize_pending);
}

static int datalab_loop_frame_phase_runtime_tick(DatalabLoopFramePhases *phase,
                                                 DatalabAppState *app_state) {
    if (!phase || !app_state) {
        return 0;
    }
    phase->panel_rescan_pending = app_state->panel_rescan_requested;
    datalab_panel_tick(app_state);
    phase->boundary_signals.sync_input_invalidated =
        phase->input_frame.invalidation.invalidation_reason_bits ? 1u : 0u;
    phase->boundary_signals.async_panel_rescan_pending =
        phase->panel_rescan_pending ? 1u : 0u;
    phase->boundary_signals.async_authoring_pending =
        app_state->workspace_authoring_pending_stub ? 1u : 0u;
    return app_state->open_picker_requested || app_state->panel_requested_pack_path[0] != '\0';
}

static uint32_t datalab_loop_frame_phase_render_decision(const DatalabLoopFramePhases *phase,
                                                         uint32_t last_present_ticks) {
    uint32_t reason_bits = 0u;
    uint32_t now_ticks = SDL_GetTicks();
    if (!phase) {
        return 0u;
    }
    if (last_present_ticks == 0u) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_FORCE;
    }
    if (phase->boundary_signals.sync_input_invalidated) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_INPUT_INVALIDATE;
    }
    if (phase->boundary_signals.async_panel_rescan_pending) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_ASYNC_PANEL_RESCAN;
    }
    if (phase->boundary_signals.async_authoring_pending) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_ASYNC_AUTHORING;
    }
    if (phase->resize_pending) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_RESIZE;
    }
    if (last_present_ticks == 0u ||
        (uint32_t)(now_ticks - last_present_ticks) >= DATALAB_LOOP_RENDER_HEARTBEAT_MS) {
        reason_bits |= DATALAB_LOOP_RENDER_REASON_HEARTBEAT;
    }
    return reason_bits;
}

static void datalab_loop_frame_phase_finalize(DatalabLoopFramePhases *phase,
                                              DatalabLoopRunState *run_state,
                                              DatalabAppState *app_state) {
    if (!phase || !run_state || !app_state) {
        return;
    }
    phase->frame_elapsed_sec = datalab_loop_elapsed_sec(phase->frame_begin_counter,
                                                        SDL_GetPerformanceCounter(),
                                                        run_state->perf_freq);
    datalab_loop_diag_tick(phase->frame_elapsed_sec, phase->wait_blocked_ms, phase->wait_call_count);
    datalab_loop_update_wait_policy(&run_state->wait_policy_input,
                                    &phase->input_frame,
                                    app_state,
                                    phase->panel_rescan_pending,
                                    phase->resize_pending);
}

static CoreResult datalab_loop_render_step_physics(SDL_Window *window,
                                                   SDL_Renderer *renderer,
                                                   const DatalabFrame *frame,
                                                   DatalabAppState *app_state,
                                                   void *lane_ctx,
                                                   DatalabRenderSubmitOutcome *out_submit) {
    DatalabPhysicsLoopContext *ctx = (DatalabPhysicsLoopContext *)lane_ctx;
    DatalabPhysicsRenderDeriveFrame render_derive;
    if (!ctx || !out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid physics loop context" };
    }
    datalab_physics_render_derive_frame(renderer,
                                        frame,
                                        app_state,
                                        ctx->density_rgba,
                                        ctx->speed_rgba,
                                        &render_derive);
    datalab_physics_render_submit_frame(window,
                                        renderer,
                                        ctx->texture,
                                        frame,
                                        app_state,
                                        ctx->segments,
                                        ctx->sample_count,
                                        &render_derive,
                                        out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_daw(SDL_Window *window,
                                               SDL_Renderer *renderer,
                                               const DatalabFrame *frame,
                                               DatalabAppState *app_state,
                                               void *lane_ctx,
                                               DatalabRenderSubmitOutcome *out_submit) {
    DatalabRenderDeriveFrame render_derive;
    (void)lane_ctx;
    if (!out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid daw loop outcome" };
    }
    datalab_render_derive_frame(frame, app_state, &render_derive);
    datalab_daw_render_submit_frame(window, renderer, frame, app_state, &render_derive, out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_trace(SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 const DatalabFrame *frame,
                                                 DatalabAppState *app_state,
                                                 void *lane_ctx,
                                                 DatalabRenderSubmitOutcome *out_submit) {
    DatalabRenderDeriveFrame render_derive;
    (void)lane_ctx;
    if (!out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid trace loop outcome" };
    }
    datalab_render_derive_frame(frame, app_state, &render_derive);
    datalab_trace_render_submit_frame(window, renderer, frame, app_state, &render_derive, out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_sketch(SDL_Window *window,
                                                  SDL_Renderer *renderer,
                                                  const DatalabFrame *frame,
                                                  DatalabAppState *app_state,
                                                  void *lane_ctx,
                                                  DatalabRenderSubmitOutcome *out_submit) {
    DatalabSketchLoopContext *ctx = (DatalabSketchLoopContext *)lane_ctx;
    DatalabSketchRenderDeriveFrame render_derive;
    if (!ctx || !out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid sketch loop context" };
    }
    datalab_sketch_render_derive_frame(renderer, frame, app_state, &render_derive);
    datalab_sketch_render_submit_frame(window,
                                       renderer,
                                       ctx->texture,
                                       frame,
                                       app_state,
                                       &render_derive,
                                       out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_run_profile(SDL_Window *window,
                                           SDL_Renderer *renderer,
                                           const DatalabFrame *frame,
                                           DatalabAppState *app_state,
                                           const DatalabLoopProfileOps *ops) {
    DatalabLoopRunState run_state = {0};
    if (!window || !renderer || !frame || !app_state || !ops || !ops->render_step) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab loop profile request" };
    }
    run_state.perf_freq = SDL_GetPerformanceFrequency();
    run_state.wait_policy_input.interaction_active = 1u;
    while (!run_state.quit) {
        DatalabLoopFramePhases phase;
        DatalabRenderSubmitOutcome render_submit = {0};
        CoreResult render_result = core_result_ok();

        datalab_loop_frame_phase_wait_and_input(&phase, &run_state, app_state);
        if (datalab_loop_frame_phase_runtime_tick(&phase, app_state)) {
            break;
        }
        datalab_loop_note_input_diag(ops->lane_tag, &run_state.ir1_diag_totals, &phase.input_frame);

        phase.render_reason_bits = datalab_loop_frame_phase_render_decision(&phase, run_state.last_present_ticks);
        phase.should_render = phase.render_reason_bits ? 1u : 0u;
        if (phase.should_render) {
            render_result = ops->render_step(window,
                                             renderer,
                                             frame,
                                             app_state,
                                             ops->lane_ctx,
                                             &render_submit);
            if (render_submit.result.code == CORE_OK && render_result.code != CORE_OK) {
                render_submit.result = render_result;
            }
            datalab_rs1_diag_note(ops->lane_tag, &run_state.rs1_diag_totals, &render_submit);
            if (render_submit.result.code != CORE_OK) {
                return render_submit.result;
            }
            if (render_submit.presented) {
                run_state.last_present_ticks = SDL_GetTicks();
            }
        }

        datalab_loop_frame_phase_finalize(&phase, &run_state, app_state);
    }
    return core_result_ok();
}

CoreResult render_physics_loop(SDL_Window *window,
                               SDL_Renderer *renderer,
                               const DatalabFrame *frame,
                               DatalabAppState *app_state) {
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             (int)frame->width,
                                             (int)frame->height);
    const size_t sample_count = (size_t)frame->width * (size_t)frame->height;
    const size_t rgba_size = sample_count * 4u;
    uint8_t *density_rgba = NULL;
    uint8_t *speed_rgba = NULL;
    float *speed = NULL;
    KitVizVecSegment *segments = NULL;
    DatalabPhysicsLoopContext physics_ctx;
    DatalabLoopProfileOps ops;
    CoreResult loop_result = core_result_ok();
    if (!texture) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }

    density_rgba = (uint8_t *)core_alloc(rgba_size);
    speed_rgba = (uint8_t *)core_alloc(rgba_size);
    speed = (float *)core_alloc(sample_count * sizeof(float));
    segments = (KitVizVecSegment *)core_alloc(sample_count * sizeof(KitVizVecSegment));
    if (!density_rgba || !speed_rgba || !speed || !segments) {
        loop_result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }

    {
        KitVizFieldStats dens_stats;
        CoreResult rs = kit_viz_compute_field_stats(frame->density, frame->width, frame->height, &dens_stats);
        if (rs.code != CORE_OK) {
            loop_result = rs;
            goto cleanup;
        }
        for (size_t i = 0; i < sample_count; ++i) {
            const float x = frame->velx[i];
            const float y = frame->vely[i];
            speed[i] = sqrtf((x * x) + (y * y));
        }
        {
            KitVizFieldStats speed_stats;
            rs = kit_viz_compute_field_stats(speed, frame->width, frame->height, &speed_stats);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
            rs = kit_viz_build_heatmap_rgba(frame->density,
                                            frame->width,
                                            frame->height,
                                            dens_stats.min_value,
                                            dens_stats.max_value,
                                            KIT_VIZ_COLORMAP_HEAT,
                                            density_rgba,
                                            rgba_size);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
            rs = kit_viz_build_heatmap_rgba(speed,
                                            frame->width,
                                            frame->height,
                                            speed_stats.min_value,
                                            speed_stats.max_value,
                                            KIT_VIZ_COLORMAP_HEAT,
                                            speed_rgba,
                                            rgba_size);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
        }
    }

    physics_ctx.texture = texture;
    physics_ctx.density_rgba = density_rgba;
    physics_ctx.speed_rgba = speed_rgba;
    physics_ctx.segments = segments;
    physics_ctx.sample_count = sample_count;

    ops.lane_tag = "physics";
    ops.lane_ctx = &physics_ctx;
    ops.render_step = datalab_loop_render_step_physics;
    loop_result = datalab_loop_run_profile(window, renderer, frame, app_state, &ops);

cleanup:
    core_free(density_rgba);
    core_free(speed_rgba);
    core_free(speed);
    core_free(segments);
    SDL_DestroyTexture(texture);
    return loop_result;
}

CoreResult render_daw_loop(SDL_Window *window,
                           SDL_Renderer *renderer,
                           const DatalabFrame *frame,
                           DatalabAppState *app_state) {
    const DatalabLoopProfileOps ops = {
        .lane_tag = "daw",
        .lane_ctx = NULL,
        .render_step = datalab_loop_render_step_daw
    };
    return datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
}

CoreResult render_sketch_loop(SDL_Window *window,
                              SDL_Renderer *renderer,
                              const DatalabFrame *frame,
                              DatalabAppState *app_state) {
    DatalabSketchLoopContext sketch_ctx;
    DatalabLoopProfileOps ops;
    CoreResult loop_result = core_result_ok();
    memset(&sketch_ctx, 0, sizeof(sketch_ctx));
    sketch_ctx.texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           (int)frame->width,
                                           (int)frame->height);
    if (!sketch_ctx.texture) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    ops.lane_tag = "sketch";
    ops.lane_ctx = &sketch_ctx;
    ops.render_step = datalab_loop_render_step_sketch;
    loop_result = datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
    SDL_DestroyTexture(sketch_ctx.texture);
    return loop_result;
}

CoreResult render_trace_loop(SDL_Window *window,
                             SDL_Renderer *renderer,
                             const DatalabFrame *frame,
                             DatalabAppState *app_state) {
    const DatalabLoopProfileOps ops = {
        .lane_tag = "trace",
        .lane_ctx = NULL,
        .render_step = datalab_loop_render_step_trace
    };
    return datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
}
