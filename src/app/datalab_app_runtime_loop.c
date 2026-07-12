#include <stdio.h>
#include <string.h>

#include "app/datalab_app_internal.h"
#include "app/datalab_runtime_pack.h"
#include "app/datalab_runtime_prefs.h"
#include "render/render_view.h"
#include "render/render_view_internal.h"

typedef struct DatalabRunLoopHandoffRequest {
    DatalabAppContext *ctx;
    DatalabDispatchRequest dispatch_request;
} DatalabRunLoopHandoffRequest;

typedef struct DatalabRunLoopHandoffOutcome {
    int dispatch_exit_code;
    int wrapper_exit_code;
} DatalabRunLoopHandoffOutcome;

static int datalab_app_dispatch_prepare_ctx(DatalabAppContext *ctx, DatalabDispatchRequest *request);
static int datalab_app_runtime_loop_adapter(const DatalabRuntimeLoopRequest *request,
                                            DatalabRuntimeLoopOutcome *outcome);
static int datalab_app_dispatch_execute_ctx(DatalabAppContext *ctx,
                                            const DatalabDispatchRequest *request,
                                            DatalabDispatchOutcome *outcome);
static int datalab_app_dispatch_finalize_ctx(DatalabAppContext *ctx, const DatalabDispatchOutcome *outcome);
static int datalab_app_run_loop_handoff_ctx(const DatalabRunLoopHandoffRequest *request,
                                            DatalabRunLoopHandoffOutcome *outcome);
static void datalab_app_release_ownership_ctx(DatalabAppContext *ctx);
static void datalab_log_render_failure(const char *label, CoreResult result);

void datalab_log_wrapper_error(const char *fn_name,
                               DatalabWrapperError wrapper_error,
                               DatalabAppStage stage,
                               int exit_code,
                               const char *detail) {
    fprintf(stderr,
            "datalab: wrapper error fn=%s code=%d stage=%d exit_code=%d detail=%s\n",
            fn_name ? fn_name : "unknown",
            (int)wrapper_error,
            (int)stage,
            exit_code,
            detail ? detail : "n/a");
}

static void datalab_log_render_failure(const char *label, CoreResult result) {
    const char *summary = datalab_render_last_failure_summary();
    if (summary && summary[0] != '\0') {
        fprintf(stderr,
                "datalab: %s failed: %s (%s)\n",
                label ? label : "render",
                result.message ? result.message : "unknown",
                summary);
        return;
    }
    fprintf(stderr,
            "datalab: %s failed: %s\n",
            label ? label : "render",
            result.message ? result.message : "unknown");
}

int datalab_app_subsystems_init(DatalabAppRuntime *runtime, DatalabAppState *app_state) {
    if (!runtime) {
        return 1;
    }
    if (runtime->no_gui) {
        return 0;
    }
    if (!app_state) {
        return 1;
    }

    datalab_runtime_copy_to_app_state(runtime, app_state, 0);
    return 0;
}

int datalab_runtime_start(DatalabAppRuntime *runtime, DatalabAppState *app_state) {
    DatalabRenderSession *render_session = NULL;
    CoreResult run_r;
    int picker_enter_authoring = 0;
    int exit_code = 0;
    if (!runtime) {
        return 1;
    }
    if (runtime->no_gui) {
        return 0;
    }
    for (;;) {
        if (!runtime->frame_loaded) {
            if (!runtime->pack_path || runtime->pack_path[0] == '\0') {
                datalab_render_session_close(render_session);
                render_session = NULL;
                run_r = datalab_render_pick_pack_path(runtime->input_root,
                                                      runtime->last_load_error,
                                                      runtime->input_root,
                                                      sizeof(runtime->input_root),
                                                      &runtime->text_zoom_step,
                                                      &runtime->workspace_authoring_theme_preset_id,
                                                      &runtime->workspace_authoring_custom_theme,
                                                      &picker_enter_authoring,
                                                      runtime->selected_pack_path,
                                                      sizeof(runtime->selected_pack_path));
                if (run_r.code != CORE_OK) {
                    fprintf(stderr, "datalab: pack picker failed: %s\n", run_r.message);
                    exit_code = 4;
                    goto cleanup;
                }
                /* Picker-only exits still own theme/text preference changes. */
                (void)datalab_runtime_prefs_save_text_zoom_step(runtime->text_zoom_step);
                (void)datalab_runtime_prefs_save_theme_preset_id(runtime->workspace_authoring_theme_preset_id);
                (void)datalab_runtime_prefs_save_custom_theme(&runtime->workspace_authoring_custom_theme);
                runtime->last_load_error[0] = '\0';
                (void)datalab_input_root_select_recent(runtime->input_root,
                                                       sizeof(runtime->input_root),
                                                       runtime->recent_input_roots,
                                                       &runtime->recent_input_root_count,
                                                       DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                       runtime->input_root);
                runtime->pack_path = runtime->selected_pack_path;
            }
            if (!runtime->pack_path || runtime->pack_path[0] == '\0') {
                datalab_runtime_prefs_save_input_root(runtime->input_root);
                exit_code = 0;
                goto cleanup;
            }
            {
                int load_rc = datalab_runtime_load_frame(runtime);
                if (load_rc != 0) {
                    if (runtime->last_load_error[0] == '\0') {
                        snprintf(runtime->last_load_error,
                                 sizeof(runtime->last_load_error),
                                 "input load failed: unsupported or invalid file");
                    }
                    runtime->pack_path = NULL;
                    runtime->selected_pack_path[0] = '\0';
                    datalab_frame_free(&runtime->frame);
                    datalab_frame_init(&runtime->frame);
                    runtime->frame_loaded = 0;
                    continue;
                }
            }
            if (!runtime->frame_loaded) {
                exit_code = 2;
                goto cleanup;
            }
            if (app_state) {
                datalab_runtime_copy_to_app_state(runtime, app_state, 1);
                if (picker_enter_authoring) {
                    datalab_workspace_authoring_begin_takeover(app_state);
                    picker_enter_authoring = 0;
                }
            }
        }

        if (!render_session) {
            run_r = datalab_render_session_open(&render_session);
            if (run_r.code != CORE_OK) {
                datalab_log_render_failure("render session", run_r);
                exit_code = 4;
                goto cleanup;
            }
        }

        if (runtime->visual_artifact_path[0] != '\0') {
            run_r = datalab_render_capture_first_frame(render_session,
                                                       &runtime->frame,
                                                       app_state,
                                                       runtime->visual_artifact_path);
            if (run_r.code != CORE_OK) {
                datalab_log_render_failure("visual artifact", run_r);
                exit_code = 4;
                goto cleanup;
            }
            printf("visual-artifact: %s\n", runtime->visual_artifact_path);
            exit_code = 0;
            goto cleanup;
        }

        run_r = datalab_render_run_with_session(render_session, &runtime->frame, app_state);
        if (app_state) {
            datalab_runtime_copy_from_app_state(runtime, app_state);
        }
        datalab_runtime_prefs_save_text_zoom_step(app_state ? app_state->text_zoom_step : runtime->text_zoom_step);
        datalab_runtime_prefs_save_theme_preset_id(runtime->workspace_authoring_theme_preset_id);
        datalab_runtime_prefs_save_custom_theme(&runtime->workspace_authoring_custom_theme);
        datalab_runtime_prefs_save_custom_theme_slots(runtime->workspace_authoring_custom_theme_slots,
                                                      DATALAB_CUSTOM_THEME_SLOT_COUNT);
        datalab_runtime_prefs_save_custom_theme_slot_names(runtime->workspace_authoring_custom_theme_slot_names,
                                                           DATALAB_CUSTOM_THEME_SLOT_COUNT);
        datalab_runtime_prefs_save_custom_theme_active_slot(runtime->workspace_authoring_custom_theme_active_slot);
        datalab_runtime_prefs_save_input_root(runtime->input_root);
        datalab_runtime_prefs_save_recent_input_roots(runtime->recent_input_roots, runtime->recent_input_root_count);
        if (run_r.code != CORE_OK) {
            datalab_log_render_failure("render", run_r);
            exit_code = 4;
            goto cleanup;
        }
        if (app_state &&
            datalab_panel_consume_requested_pack_path(app_state,
                                                      runtime->selected_pack_path,
                                                      sizeof(runtime->selected_pack_path))) {
            runtime->pack_path = runtime->selected_pack_path;
        } else if (app_state && app_state->open_picker_requested) {
            datalab_render_session_close(render_session);
            render_session = NULL;
            runtime->pack_path = NULL;
            app_state->open_picker_requested = 0;
            (void)datalab_input_root_select_recent(runtime->input_root,
                                                   sizeof(runtime->input_root),
                                                   runtime->recent_input_roots,
                                                   &runtime->recent_input_root_count,
                                                   DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                   runtime->input_root);
        } else {
            exit_code = 0;
            goto cleanup;
        }
        if (runtime->frame_loaded) {
            datalab_frame_free(&runtime->frame);
            datalab_frame_init(&runtime->frame);
            runtime->frame_loaded = 0;
        }
    }

cleanup:
    datalab_render_session_close(render_session);
    return exit_code;
}

int datalab_default_runtime_dispatch(const DatalabDispatchRequest *request,
                                     DatalabDispatchOutcome *outcome) {
    DatalabAppState app_state;
    int rc = 0;
    if (!request || !request->runtime || !outcome) {
        return 1;
    }
    memset(outcome, 0, sizeof(*outcome));
    rc = datalab_app_subsystems_init(request->runtime, &app_state);
    if (rc != 0) {
        outcome->exit_code = rc;
        return 0;
    }
    outcome->runtime_started = 1;
    rc = datalab_runtime_start(request->runtime, &app_state);
    outcome->exit_code = rc;
    outcome->dispatched = 1;
    return 0;
}

static int datalab_app_dispatch_prepare_ctx(DatalabAppContext *ctx, DatalabDispatchRequest *request) {
    if (!ctx || !request || !ctx->runtime || !ctx->runtime_dispatch) {
        return 1;
    }
    if (ctx->stage != DATALAB_APP_STAGE_STATE_SEEDED) {
        return 1;
    }
    memset(request, 0, sizeof(*request));
    request->runtime = ctx->runtime;
    ctx->dispatch_summary.dispatch_count = 1u;
    ctx->ownership.dispatch_owned = 1;
    return 0;
}

static int datalab_app_runtime_loop_adapter(const DatalabRuntimeLoopRequest *request,
                                            DatalabRuntimeLoopOutcome *outcome) {
    DatalabDispatchOutcome dispatch_outcome;
    int rc = 0;
    if (!request || !outcome || !request->dispatch_request || !request->runtime_dispatch) {
        return 1;
    }
    memset(outcome, 0, sizeof(*outcome));
    memset(&dispatch_outcome, 0, sizeof(dispatch_outcome));
    rc = request->runtime_dispatch(request->dispatch_request, &dispatch_outcome);
    if (rc != 0) {
        return rc;
    }
    outcome->exit_code = dispatch_outcome.exit_code;
    outcome->dispatched = dispatch_outcome.dispatched;
    outcome->runtime_started = dispatch_outcome.runtime_started;
    return 0;
}

static int datalab_app_dispatch_execute_ctx(DatalabAppContext *ctx,
                                            const DatalabDispatchRequest *request,
                                            DatalabDispatchOutcome *outcome) {
    DatalabRuntimeLoopRequest loop_request;
    DatalabRuntimeLoopOutcome loop_outcome;
    int rc = 0;
    if (!ctx || !request || !outcome || !ctx->runtime_dispatch) {
        return 1;
    }
    memset(outcome, 0, sizeof(*outcome));
    memset(&loop_request, 0, sizeof(loop_request));
    memset(&loop_outcome, 0, sizeof(loop_outcome));
    loop_request.dispatch_request = request;
    loop_request.runtime_dispatch = ctx->runtime_dispatch;
    rc = datalab_app_runtime_loop_adapter(&loop_request, &loop_outcome);
    if (rc != 0) {
        return rc;
    }
    outcome->exit_code = loop_outcome.exit_code;
    outcome->dispatched = loop_outcome.dispatched;
    outcome->runtime_started = loop_outcome.runtime_started;
    return 0;
}

static int datalab_app_dispatch_finalize_ctx(DatalabAppContext *ctx, const DatalabDispatchOutcome *outcome) {
    if (!ctx || !outcome) {
        return 1;
    }
    ctx->dispatch_summary.dispatch_succeeded = outcome->dispatched ? 1 : 0;
    ctx->dispatch_summary.last_dispatch_exit_code = outcome->exit_code;
    if (outcome->runtime_started) {
        ctx->ownership.runtime_owned = 1;
        if (!datalab_app_transition_stage(ctx,
                                          DATALAB_APP_STAGE_STATE_SEEDED,
                                          DATALAB_APP_STAGE_RUNTIME_STARTED,
                                          "datalab_runtime_start",
                                          __func__)) {
            return 1;
        }
    }
    if (!datalab_app_transition_stage(ctx,
                                      DATALAB_APP_STAGE_RUNTIME_STARTED,
                                      DATALAB_APP_STAGE_LOOP_COMPLETED,
                                      "datalab_app_run_loop",
                                      __func__)) {
        return 1;
    }
    return outcome->exit_code;
}

static int datalab_app_run_loop_handoff_ctx(const DatalabRunLoopHandoffRequest *request,
                                            DatalabRunLoopHandoffOutcome *outcome) {
    DatalabDispatchOutcome dispatch_outcome;
    int rc = 0;
    if (!request || !outcome || !request->ctx || !request->dispatch_request.runtime) {
        if (outcome) {
            memset(outcome, 0, sizeof(*outcome));
            outcome->wrapper_exit_code = DATALAB_WRAPPER_ERROR_RUN_LOOP_HANDOFF_FAILED;
        }
        if (request && request->ctx) {
            request->ctx->wrapper_error = DATALAB_WRAPPER_ERROR_RUN_LOOP_HANDOFF_FAILED;
            datalab_log_wrapper_error(__func__,
                                      request->ctx->wrapper_error,
                                      request->ctx->stage,
                                      DATALAB_WRAPPER_ERROR_RUN_LOOP_HANDOFF_FAILED,
                                      "invalid handoff request");
        }
        return 1;
    }
    memset(outcome, 0, sizeof(*outcome));
    memset(&dispatch_outcome, 0, sizeof(dispatch_outcome));
    request->ctx->ownership.run_loop_handoff_owned = 1;
    rc = datalab_app_dispatch_execute_ctx(request->ctx, &request->dispatch_request, &dispatch_outcome);
    if (rc != 0) {
        request->ctx->dispatch_summary.dispatch_succeeded = 0;
        request->ctx->dispatch_summary.last_dispatch_exit_code = rc;
        request->ctx->wrapper_error = DATALAB_WRAPPER_ERROR_DISPATCH_EXECUTE_FAILED;
        datalab_log_wrapper_error(__func__,
                                  request->ctx->wrapper_error,
                                  request->ctx->stage,
                                  rc,
                                  "dispatch execute failed");
        outcome->wrapper_exit_code = rc;
        return 1;
    }
    outcome->dispatch_exit_code = dispatch_outcome.exit_code;
    outcome->wrapper_exit_code = datalab_app_dispatch_finalize_ctx(request->ctx, &dispatch_outcome);
    return 0;
}

int datalab_app_run_loop_ctx(DatalabAppContext *ctx) {
    DatalabDispatchRequest dispatch_request;
    DatalabRunLoopHandoffRequest handoff_request;
    DatalabRunLoopHandoffOutcome handoff_outcome;
    int rc = datalab_app_dispatch_prepare_ctx(ctx, &dispatch_request);
    if (rc != 0) {
        ctx->wrapper_error = DATALAB_WRAPPER_ERROR_DISPATCH_PREPARE_FAILED;
        datalab_log_wrapper_error(__func__,
                                  ctx->wrapper_error,
                                  ctx->stage,
                                  rc,
                                  "dispatch prepare failed");
        return rc;
    }
    memset(&handoff_request, 0, sizeof(handoff_request));
    memset(&handoff_outcome, 0, sizeof(handoff_outcome));
    handoff_request.ctx = ctx;
    handoff_request.dispatch_request = dispatch_request;
    if (datalab_app_run_loop_handoff_ctx(&handoff_request, &handoff_outcome) != 0) {
        if (handoff_outcome.wrapper_exit_code == 0) {
            return DATALAB_WRAPPER_ERROR_RUN_LOOP_HANDOFF_FAILED;
        }
        return handoff_outcome.wrapper_exit_code;
    }
    if (handoff_outcome.wrapper_exit_code != 0) {
        ctx->wrapper_error = DATALAB_WRAPPER_ERROR_DISPATCH_FINALIZE_FAILED;
        datalab_log_wrapper_error(__func__,
                                  ctx->wrapper_error,
                                  ctx->stage,
                                  handoff_outcome.wrapper_exit_code,
                                  "dispatch finalize returned non-zero");
    }
    return handoff_outcome.wrapper_exit_code;
}

static void datalab_app_release_ownership_ctx(DatalabAppContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->ownership.run_loop_handoff_owned = 0;
    ctx->ownership.runtime_owned = 0;
    ctx->ownership.dispatch_owned = 0;
    ctx->ownership.state_seed_owned = 0;
    ctx->ownership.config_owned = 0;
    ctx->ownership.bootstrap_owned = 0;
}

int datalab_app_run_loop(DatalabAppRuntime *runtime) {
    DatalabAppContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = runtime;
    ctx.stage = DATALAB_APP_STAGE_STATE_SEEDED;
    ctx.runtime_dispatch = datalab_default_runtime_dispatch;
    return datalab_app_run_loop_ctx(&ctx);
}

void datalab_app_shutdown_ctx(DatalabAppContext *ctx) {
    DatalabAppRuntime *runtime = NULL;
    if (!ctx || !ctx->runtime) {
        return;
    }
    runtime = ctx->runtime;
    if (runtime->frame_loaded) {
        datalab_frame_free(&runtime->frame);
        runtime->frame_loaded = 0;
    }
    datalab_runtime_reset_prefetch(runtime);
    datalab_app_release_ownership_ctx(ctx);
    ctx->ownership.shutdown_owned = 1;
    ctx->stage = DATALAB_APP_STAGE_SHUTDOWN_COMPLETED;
}

void datalab_app_shutdown(DatalabAppRuntime *runtime) {
    DatalabAppContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = runtime;
    ctx.stage = DATALAB_APP_STAGE_LOOP_COMPLETED;
    datalab_app_shutdown_ctx(&ctx);
}
