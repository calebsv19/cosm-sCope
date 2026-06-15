#include "render/render_view_internal.h"

#include <stdio.h>
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
    if (datalab_workspace_authoring_route_mouse_event(event, app_state)) {
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
    phase->panel_rescan_pending = app_state->panel_rescan_requested;
    datalab_session_controls_tick(app_state);
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
