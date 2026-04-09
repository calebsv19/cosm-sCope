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

CoreResult render_physics_loop(SDL_Window *window,
                               SDL_Renderer *renderer,
                               const DatalabFrame *frame,
                               DatalabAppState *app_state) {
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             (int)frame->width,
                                             (int)frame->height);
    if (!texture) {
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }

    const size_t sample_count = (size_t)frame->width * (size_t)frame->height;
    const size_t rgba_size = sample_count * 4u;

    uint8_t *density_rgba = (uint8_t *)core_alloc(rgba_size);
    uint8_t *speed_rgba = (uint8_t *)core_alloc(rgba_size);
    float *speed = (float *)core_alloc(sample_count * sizeof(float));
    KitVizVecSegment *segments = (KitVizVecSegment *)core_alloc(sample_count * sizeof(KitVizVecSegment));
    if (!density_rgba || !speed_rgba || !speed || !segments) {
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    KitVizFieldStats dens_stats;
    CoreResult rs = kit_viz_compute_field_stats(frame->density, frame->width, frame->height, &dens_stats);
    if (rs.code != CORE_OK) {
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
    }

    for (size_t i = 0; i < sample_count; ++i) {
        const float x = frame->velx[i];
        const float y = frame->vely[i];
        speed[i] = sqrtf((x * x) + (y * y));
    }

    KitVizFieldStats speed_stats;
    rs = kit_viz_compute_field_stats(speed, frame->width, frame->height, &speed_stats);
    if (rs.code != CORE_OK) {
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
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
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
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
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return rs;
    }

    int quit = 0;
    DatalabInputDiagTotals ir1_diag_totals = {0};
    DatalabRenderDiagTotals rs1_diag_totals = {0};
    while (!quit) {
        SDL_Event e;
        DatalabInputFrame input_frame;
        DatalabPhysicsRenderDeriveFrame render_derive;
        DatalabRenderSubmitOutcome render_submit;
        datalab_input_frame_begin(&input_frame);
        while (SDL_PollEvent(&e)) {
            datalab_input_apply_event(&input_frame, &e);
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type == SDL_KEYDOWN) datalab_handle_keydown(&e.key, app_state, &quit);
        }
        datalab_panel_tick(app_state);
        if (app_state->open_picker_requested || app_state->panel_requested_pack_path[0] != '\0') {
            break;
        }
        ir1_diag_totals.frame_count += 1u;
        ir1_diag_totals.event_count_total += input_frame.raw.sdl_event_count;
        ir1_diag_totals.routed_global_total += input_frame.route.routed_global_count;
        ir1_diag_totals.routed_fallback_total += input_frame.route.routed_fallback_count;
        ir1_diag_totals.invalidation_reason_bits_total += input_frame.invalidation.invalidation_reason_bits;
        if (datalab_ir1_diag_enabled()) {
            printf("[ir1] datalab-physics frame=%llu events=%u route(global=%u fallback=%u target=%d) "
                   "invalidate(bits=0x%x target=%u full=%u) totals(frames=%llu events=%llu global=%llu fallback=%llu invalid_bits_sum=%llu)\n",
                   (unsigned long long)ir1_diag_totals.frame_count,
                   (unsigned int)input_frame.raw.sdl_event_count,
                   (unsigned int)input_frame.route.routed_global_count,
                   (unsigned int)input_frame.route.routed_fallback_count,
                   (int)input_frame.route.target_policy,
                   (unsigned int)input_frame.invalidation.invalidation_reason_bits,
                   (unsigned int)input_frame.invalidation.target_invalidation_count,
                   (unsigned int)input_frame.invalidation.full_invalidation_count,
                   (unsigned long long)ir1_diag_totals.frame_count,
                   (unsigned long long)ir1_diag_totals.event_count_total,
                   (unsigned long long)ir1_diag_totals.routed_global_total,
                   (unsigned long long)ir1_diag_totals.routed_fallback_total,
                   (unsigned long long)ir1_diag_totals.invalidation_reason_bits_total);
        }
        datalab_physics_render_derive_frame(renderer,
                                            frame,
                                            app_state,
                                            density_rgba,
                                            speed_rgba,
                                            &render_derive);
        datalab_physics_render_submit_frame(window,
                                            renderer,
                                            texture,
                                            frame,
                                            app_state,
                                            segments,
                                            sample_count,
                                            &render_derive,
                                            &render_submit);
        datalab_rs1_diag_note("physics", &rs1_diag_totals, &render_submit);
        if (render_submit.result.code != CORE_OK) {
            core_free(density_rgba);
            core_free(speed_rgba);
            core_free(speed);
            core_free(segments);
            SDL_DestroyTexture(texture);
            return render_submit.result;
        }
    }

    core_free(density_rgba);
    core_free(speed_rgba);
    core_free(speed);
    core_free(segments);
    SDL_DestroyTexture(texture);
    return core_result_ok();
}

CoreResult render_daw_loop(SDL_Window *window,
                           SDL_Renderer *renderer,
                           const DatalabFrame *frame,
                           DatalabAppState *app_state) {
    int quit = 0;
    DatalabInputDiagTotals ir1_diag_totals = {0};
    DatalabRenderDiagTotals rs1_diag_totals = {0};
    while (!quit) {
        SDL_Event e;
        DatalabInputFrame input_frame;
        DatalabRenderDeriveFrame render_derive;
        DatalabRenderSubmitOutcome render_submit;
        datalab_input_frame_begin(&input_frame);
        while (SDL_PollEvent(&e)) {
            datalab_input_apply_event(&input_frame, &e);
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            if (e.type == SDL_KEYDOWN) {
                datalab_handle_keydown(&e.key, app_state, &quit);
            }
        }
        datalab_panel_tick(app_state);
        if (app_state->open_picker_requested || app_state->panel_requested_pack_path[0] != '\0') {
            break;
        }
        ir1_diag_totals.frame_count += 1u;
        ir1_diag_totals.event_count_total += input_frame.raw.sdl_event_count;
        ir1_diag_totals.routed_global_total += input_frame.route.routed_global_count;
        ir1_diag_totals.routed_fallback_total += input_frame.route.routed_fallback_count;
        ir1_diag_totals.invalidation_reason_bits_total += input_frame.invalidation.invalidation_reason_bits;
        if (datalab_ir1_diag_enabled()) {
            printf("[ir1] datalab-daw frame=%llu events=%u route(global=%u fallback=%u target=%d) "
                   "invalidate(bits=0x%x target=%u full=%u) totals(frames=%llu events=%llu global=%llu fallback=%llu invalid_bits_sum=%llu)\n",
                   (unsigned long long)ir1_diag_totals.frame_count,
                   (unsigned int)input_frame.raw.sdl_event_count,
                   (unsigned int)input_frame.route.routed_global_count,
                   (unsigned int)input_frame.route.routed_fallback_count,
                   (int)input_frame.route.target_policy,
                   (unsigned int)input_frame.invalidation.invalidation_reason_bits,
                   (unsigned int)input_frame.invalidation.target_invalidation_count,
                   (unsigned int)input_frame.invalidation.full_invalidation_count,
                   (unsigned long long)ir1_diag_totals.frame_count,
                   (unsigned long long)ir1_diag_totals.event_count_total,
                   (unsigned long long)ir1_diag_totals.routed_global_total,
                   (unsigned long long)ir1_diag_totals.routed_fallback_total,
                   (unsigned long long)ir1_diag_totals.invalidation_reason_bits_total);
        }
        datalab_render_derive_frame(frame, app_state, &render_derive);
        datalab_daw_render_submit_frame(window, renderer, frame, app_state, &render_derive, &render_submit);
        datalab_rs1_diag_note("daw", &rs1_diag_totals, &render_submit);
        if (render_submit.result.code != CORE_OK) {
            return render_submit.result;
        }
    }

    return core_result_ok();
}

CoreResult render_trace_loop(SDL_Window *window,
                             SDL_Renderer *renderer,
                             const DatalabFrame *frame,
                             DatalabAppState *app_state) {
    int quit = 0;
    DatalabInputDiagTotals ir1_diag_totals = {0};
    DatalabRenderDiagTotals rs1_diag_totals = {0};
    while (!quit) {
        SDL_Event e;
        DatalabInputFrame input_frame;
        DatalabRenderDeriveFrame render_derive;
        DatalabRenderSubmitOutcome render_submit;
        datalab_input_frame_begin(&input_frame);
        while (SDL_PollEvent(&e)) {
            datalab_input_apply_event(&input_frame, &e);
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            if (e.type == SDL_KEYDOWN) {
                datalab_handle_keydown(&e.key, app_state, &quit);
            }
        }
        datalab_panel_tick(app_state);
        if (app_state->open_picker_requested || app_state->panel_requested_pack_path[0] != '\0') {
            break;
        }
        ir1_diag_totals.frame_count += 1u;
        ir1_diag_totals.event_count_total += input_frame.raw.sdl_event_count;
        ir1_diag_totals.routed_global_total += input_frame.route.routed_global_count;
        ir1_diag_totals.routed_fallback_total += input_frame.route.routed_fallback_count;
        ir1_diag_totals.invalidation_reason_bits_total += input_frame.invalidation.invalidation_reason_bits;
        if (datalab_ir1_diag_enabled()) {
            printf("[ir1] datalab-trace frame=%llu events=%u route(global=%u fallback=%u target=%d) "
                   "invalidate(bits=0x%x target=%u full=%u) totals(frames=%llu events=%llu global=%llu fallback=%llu invalid_bits_sum=%llu)\n",
                   (unsigned long long)ir1_diag_totals.frame_count,
                   (unsigned int)input_frame.raw.sdl_event_count,
                   (unsigned int)input_frame.route.routed_global_count,
                   (unsigned int)input_frame.route.routed_fallback_count,
                   (int)input_frame.route.target_policy,
                   (unsigned int)input_frame.invalidation.invalidation_reason_bits,
                   (unsigned int)input_frame.invalidation.target_invalidation_count,
                   (unsigned int)input_frame.invalidation.full_invalidation_count,
                   (unsigned long long)ir1_diag_totals.frame_count,
                   (unsigned long long)ir1_diag_totals.event_count_total,
                   (unsigned long long)ir1_diag_totals.routed_global_total,
                   (unsigned long long)ir1_diag_totals.routed_fallback_total,
                   (unsigned long long)ir1_diag_totals.invalidation_reason_bits_total);
        }
        datalab_render_derive_frame(frame, app_state, &render_derive);
        datalab_trace_render_submit_frame(window, renderer, frame, app_state, &render_derive, &render_submit);
        datalab_rs1_diag_note("trace", &rs1_diag_totals, &render_submit);
        if (render_submit.result.code != CORE_OK) {
            return render_submit.result;
        }
    }
    return core_result_ok();
}
