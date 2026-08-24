#include "render/render_view_internal.h"
#include "render/datalab_render_perf_diag.h"
#include "app/datalab_async_decode.h"
#include "app/datalab_runtime_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui/input.h"

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

static double datalab_loop_elapsed_sec(Uint64 begin_counter,
                                       Uint64 end_counter,
                                       Uint64 perf_freq) {
    if (perf_freq == 0u || end_counter <= begin_counter) {
        return 0.0;
    }
    return (double)(end_counter - begin_counter) / (double)perf_freq;
}

static void datalab_loop_handle_event(SDL_Window *window,
                                      SDL_Renderer *renderer,
                                      const SDL_Event *event,
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
    if (datalab_workspace_authoring_route_mouse_event(window, renderer, event, app_state)) {
        return;
    }
    if (datalab_playback_hud_route_mouse_event(window, renderer, event, app_state)) {
        return;
    }
    if (datalab_session_controls_route_mouse_event(window, renderer, event, app_state)) {
        return;
    }
    if (datalab_handle_mouse_event(window, renderer, event, app_state)) {
        return;
    }
    if (event->type == SDL_KEYDOWN) {
        datalab_workspace_authoring_route_keydown(&event->key, app_state, &authoring_route);
        if (!authoring_route.consumed) {
            datalab_handle_keydown(&event->key, app_state, quit);
        }
    }
}

static void datalab_loop_input_wait_and_drain(SDL_Window *window,
                                              SDL_Renderer *renderer,
                                              DatalabInputFrame *input_frame,
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
            datalab_loop_handle_event(window, renderer, &event, input_frame, app_state, quit, out_resize_pending);
        }
        *out_wait_blocked_ms += (SDL_GetTicks() - wait_start);
        *out_wait_call_count += 1u;
    }
    while (SDL_PollEvent(&event)) {
        datalab_loop_handle_event(window, renderer, &event, input_frame, app_state, quit, out_resize_pending);
    }
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

static void datalab_loop_frame_phase_wait_and_input(SDL_Window *window,
                                                    SDL_Renderer *renderer,
                                                    DatalabLoopFramePhases *phase,
                                                    DatalabLoopRunState *run_state,
                                                    DatalabAppState *app_state) {
    if (!phase || !run_state || !app_state) {
        return;
    }
    memset(phase, 0, sizeof(*phase));
    phase->frame_begin_counter = SDL_GetPerformanceCounter();
    phase->wait_timeout_ms = datalab_loop_compute_wait_timeout_ms(&run_state->wait_policy_input);
    datalab_input_frame_begin(&phase->input_frame);
    datalab_loop_input_wait_and_drain(window,
                                      renderer,
                                      &phase->input_frame,
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
    if (app_state->async_decode && app_state->runtime_owner) {
        (void)datalab_async_decode_pump(app_state->async_decode, app_state->runtime_owner, app_state);
    }
    if (datalab_workspace_authoring_runtime_mutation_allowed(app_state)) {
        phase->panel_rescan_pending = app_state->panel_rescan_requested;
        datalab_session_controls_tick(app_state);
        if (app_state->async_decode && app_state->panel_requested_pack_path[0] != '\0' &&
            datalab_runtime_focus_request(app_state->runtime_owner,
                                          app_state,
                                          app_state->panel_requested_pack_path)) {
            app_state->panel_requested_pack_path[0] = '\0';
        }
    }
    phase->boundary_signals.sync_input_invalidated =
        phase->input_frame.invalidation.invalidation_reason_bits ? 1u : 0u;
    phase->boundary_signals.async_decode_frame_ready =
        datalab_async_decode_pending_selected(app_state->async_decode) ? 1u : 0u;
    app_state->async_decode_frame_ready = phase->boundary_signals.async_decode_frame_ready;
    phase->boundary_signals.async_panel_rescan_pending =
        phase->panel_rescan_pending ? 1u : 0u;
    phase->boundary_signals.async_authoring_pending =
        app_state->workspace_authoring_pending_stub ? 1u : 0u;
    return datalab_workspace_authoring_runtime_mutation_allowed(app_state) &&
            (app_state->open_picker_requested || app_state->panel_requested_pack_path[0] != '\0');
}

static uint32_t datalab_loop_frame_phase_render_decision(const DatalabLoopFramePhases *phase,
                                                         uint32_t last_present_ticks) {
    if (!phase) {
        return 0u;
    }
    return datalab_loop_compute_render_reason_bits(&phase->boundary_signals,
                                                   phase->resize_pending,
                                                   last_present_ticks,
                                                   SDL_GetTicks());
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
    datalab_loop_update_wait_policy_input(&run_state->wait_policy_input,
                                          &phase->input_frame,
                                          app_state,
                                          phase->panel_rescan_pending,
                                          phase->resize_pending);
}

CoreResult datalab_loop_run_profile(SDL_Window *window,
                                    SDL_Renderer *renderer,
                                    const DatalabFrame *frame,
                                    DatalabAppState *app_state,
                                    const DatalabLoopProfileOps *ops) {
    DatalabLoopRunState run_state = {0};
    const int native_reuse_proof = frame && frame->profile == DATALAB_PROFILE_IMAGE &&
                                   getenv("DATALAB_NATIVE_IMAGE_REUSE_PROOF") != NULL;
    uint32_t native_reuse_proof_present_count = 0u;
    uint64_t proof_image_upload_baseline = 0u;
    uint64_t proof_image_reuse_baseline = 0u;
    uint64_t proof_overlay_upload_baseline = 0u;
    uint64_t proof_overlay_reuse_baseline = 0u;
    uint64_t proof_overlay_redraw_baseline = 0u;
    uint64_t proof_overlay_redraw_reuse_baseline = 0u;
    if (!window || !renderer || !frame || !app_state || !ops || !ops->render_step) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid datalab loop profile request" };
    }
    run_state.perf_freq = SDL_GetPerformanceFrequency();
    run_state.wait_policy_input.interaction_active = 1u;
    while (!run_state.quit) {
        DatalabLoopFramePhases phase;
        DatalabRenderSubmitOutcome render_submit = {0};
        CoreResult render_result = core_result_ok();

        datalab_loop_frame_phase_wait_and_input(window, renderer, &phase, &run_state, app_state);
        if (datalab_loop_frame_phase_runtime_tick(&phase, app_state)) {
            break;
        }
        datalab_loop_note_input_diag(ops->lane_tag, &run_state.ir1_diag_totals, &phase.input_frame);

        phase.render_reason_bits = datalab_loop_frame_phase_render_decision(&phase, run_state.last_present_ticks);
        phase.should_render = phase.render_reason_bits ? 1u : 0u;
        if (phase.should_render) {
            datalab_render_perf_diag_begin_frame((int)frame->profile,
                                                 phase.render_reason_bits,
                                                 frame->width,
                                                 frame->height);
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
                datalab_render_perf_diag_finish(0,
                                                DATALAB_RENDER_PERF_STAGE_SOFTWARE_SUBMIT,
                                                (int)render_submit.result.code);
                return render_submit.result;
            }
            if (render_submit.presented) {
                run_state.last_present_ticks = SDL_GetTicks();
                if (native_reuse_proof) {
                    uint64_t image_upload_count = 0u;
                    uint64_t image_reuse_count = 0u;
                    uint64_t overlay_upload_count = 0u;
                    uint64_t overlay_reuse_count = 0u;
                    uint64_t overlay_redraw_count = 0u;
                    uint64_t overlay_redraw_reuse_count = 0u;
                    if (!datalab_renderer_backend_native_image_counters(
                            renderer,
                            &image_upload_count,
                            &image_reuse_count,
                            &overlay_upload_count,
                            &overlay_reuse_count,
                            &overlay_redraw_count,
                            &overlay_redraw_reuse_count)) {
                        return (CoreResult){CORE_ERR_IO,
                                           "native image reuse proof requires Vulkan image counters"};
                    }
                    native_reuse_proof_present_count += 1u;
                    if (native_reuse_proof_present_count == 1u) {
                        SDL_Event zoom_event = {0};
                        int window_width = 0;
                        int window_height = 0;
                        proof_image_upload_baseline = image_upload_count;
                        proof_image_reuse_baseline = image_reuse_count;
                        proof_overlay_upload_baseline = overlay_upload_count;
                        proof_overlay_reuse_baseline = overlay_reuse_count;
                        proof_overlay_redraw_baseline = overlay_redraw_count;
                        proof_overlay_redraw_reuse_baseline = overlay_redraw_reuse_count;
                        SDL_GetWindowSize(window, &window_width, &window_height);
                        SDL_WarpMouseInWindow(window, window_width / 2, window_height / 2);
                        zoom_event.type = SDL_MOUSEWHEEL;
                        zoom_event.wheel.y = 1;
                        zoom_event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
                        if (SDL_PushEvent(&zoom_event) < 0) {
                            return (CoreResult){CORE_ERR_IO,
                                               "native image reuse proof could not queue zoom"};
                        }
                    } else if (native_reuse_proof_present_count == 2u) {
                        SDL_Event quit_event = {0};
                        const uint64_t image_upload_delta =
                            image_upload_count - proof_image_upload_baseline;
                        const uint64_t image_reuse_delta =
                            image_reuse_count - proof_image_reuse_baseline;
                        const uint64_t overlay_upload_delta =
                            overlay_upload_count - proof_overlay_upload_baseline;
                        const uint64_t overlay_reuse_delta =
                            overlay_reuse_count - proof_overlay_reuse_baseline;
                        const uint64_t overlay_redraw_delta =
                            overlay_redraw_count - proof_overlay_redraw_baseline;
                        const uint64_t overlay_redraw_reuse_delta =
                            overlay_redraw_reuse_count - proof_overlay_redraw_reuse_baseline;
                        const int passed = image_upload_delta == 0u &&
                                           overlay_upload_delta == 0u &&
                                           overlay_redraw_delta == 0u &&
                                           image_reuse_delta >= 1u &&
                                           overlay_reuse_delta >= 1u &&
                                           overlay_redraw_reuse_delta >= 1u;
                        fprintf(stdout,
                                "DATALAB_NATIVE_IMAGE_REUSE schema=2 status=%s image_upload_delta=%llu image_reuse_delta=%llu compatibility_upload_delta=%llu overlay_reuse_delta=%llu overlay_redraw_delta=%llu overlay_redraw_reuse_delta=%llu\n",
                                passed ? "pass" : "fail",
                                (unsigned long long)image_upload_delta,
                                (unsigned long long)image_reuse_delta,
                                (unsigned long long)overlay_upload_delta,
                                (unsigned long long)overlay_reuse_delta,
                                (unsigned long long)overlay_redraw_delta,
                                (unsigned long long)overlay_redraw_reuse_delta);
                        if (!passed) {
                            return (CoreResult){CORE_ERR_IO,
                                               "native image stable zoom performed an upload"};
                        }
                        quit_event.type = SDL_QUIT;
                        if (SDL_PushEvent(&quit_event) < 0) {
                            return (CoreResult){CORE_ERR_IO,
                                               "native image reuse proof could not queue quit"};
                        }
                    }
                }
            }
        }

        datalab_loop_frame_phase_finalize(&phase, &run_state, app_state);
    }
    return core_result_ok();
}
