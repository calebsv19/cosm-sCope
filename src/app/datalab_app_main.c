#include "datalab/datalab_app_main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/datalab_runtime_pack.h"
#include "app/datalab_runtime_prefs.h"
#include "core_data.h"
#include "data/dataset_builders.h"
#include "render/render_view.h"

static const char *k_datalab_default_input_root = "data/import";

typedef enum DatalabAppStage {
    DATALAB_APP_STAGE_INIT = 0,
    DATALAB_APP_STAGE_BOOTSTRAPPED,
    DATALAB_APP_STAGE_CONFIG_LOADED,
    DATALAB_APP_STAGE_STATE_SEEDED,
    DATALAB_APP_STAGE_RUNTIME_STARTED,
    DATALAB_APP_STAGE_LOOP_COMPLETED,
    DATALAB_APP_STAGE_SHUTDOWN_COMPLETED
} DatalabAppStage;

typedef enum DatalabWrapperError {
    DATALAB_WRAPPER_ERROR_NONE = 0,
    DATALAB_WRAPPER_ERROR_BOOTSTRAP_FAILED = 1,
    DATALAB_WRAPPER_ERROR_CONFIG_LOAD_FAILED = 2,
    DATALAB_WRAPPER_ERROR_STATE_SEED_FAILED = 3,
    DATALAB_WRAPPER_ERROR_DISPATCH_PREPARE_FAILED = 4,
    DATALAB_WRAPPER_ERROR_DISPATCH_EXECUTE_FAILED = 5,
    DATALAB_WRAPPER_ERROR_DISPATCH_FINALIZE_FAILED = 6,
    DATALAB_WRAPPER_ERROR_STAGE_TRANSITION_FAILED = 7,
    DATALAB_WRAPPER_ERROR_RUN_LOOP_HANDOFF_FAILED = 8
} DatalabWrapperError;

typedef struct DatalabDispatchRequest {
    DatalabAppRuntime *runtime;
} DatalabDispatchRequest;

typedef struct DatalabDispatchOutcome {
    int exit_code;
    int dispatched;
    int runtime_started;
} DatalabDispatchOutcome;

typedef struct DatalabRuntimeLoopRequest {
    const DatalabDispatchRequest *dispatch_request;
    int (*runtime_dispatch)(const DatalabDispatchRequest *request, DatalabDispatchOutcome *outcome);
} DatalabRuntimeLoopRequest;

typedef struct DatalabRuntimeLoopOutcome {
    int exit_code;
    int dispatched;
    int runtime_started;
} DatalabRuntimeLoopOutcome;

typedef struct DatalabAppContext DatalabAppContext;

typedef struct DatalabRunLoopHandoffRequest {
    DatalabAppContext *ctx;
    DatalabDispatchRequest dispatch_request;
} DatalabRunLoopHandoffRequest;

typedef struct DatalabRunLoopHandoffOutcome {
    int dispatch_exit_code;
    int wrapper_exit_code;
} DatalabRunLoopHandoffOutcome;

typedef struct DatalabDispatchSummary {
    unsigned int dispatch_count;
    int dispatch_succeeded;
    int last_dispatch_exit_code;
} DatalabDispatchSummary;

typedef struct DatalabLifecycleOwnership {
    int bootstrap_owned;
    int config_owned;
    int state_seed_owned;
    int runtime_owned;
    int dispatch_owned;
    int run_loop_handoff_owned;
    int shutdown_owned;
} DatalabLifecycleOwnership;

struct DatalabAppContext {
    DatalabAppRuntime *runtime;
    DatalabAppStage stage;
    int (*runtime_dispatch)(const DatalabDispatchRequest *request, DatalabDispatchOutcome *outcome);
    DatalabDispatchSummary dispatch_summary;
    DatalabLifecycleOwnership ownership;
    DatalabWrapperError wrapper_error;
    int exit_code;
};

static int datalab_default_runtime_dispatch(const DatalabDispatchRequest *request,
                                            DatalabDispatchOutcome *outcome);

static void datalab_log_wrapper_error(const char *fn_name,
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

static int datalab_app_transition_stage(DatalabAppContext *ctx,
                                        DatalabAppStage expected,
                                        DatalabAppStage next,
                                        const char *stage_name,
                                        const char *fn_name) {
    if (!ctx) {
        return 0;
    }
    if (ctx->stage != expected) {
        fprintf(stderr,
                "datalab: stage transition violation fn=%s stage=%s expected=%d actual=%d next=%d\n",
                fn_name ? fn_name : "unknown",
                stage_name ? stage_name : "unknown",
                (int)expected,
                (int)ctx->stage,
                (int)next);
        return 0;
    }
    ctx->stage = next;
    return 1;
}

static void datalab_print_usage(const char *argv0) {
    printf("usage: %s [--pack /path/to/frame.pack] [--input-root /path/to/folder] [--no-gui]\n", argv0);
}

void datalab_app_runtime_init(DatalabAppRuntime *runtime) {
    int i = 0;
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    datalab_frame_init(&runtime->frame);
    snprintf(runtime->input_root, sizeof(runtime->input_root), "%s", k_datalab_default_input_root);
    runtime->workspace_authoring_theme_preset_id =
        (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    runtime->workspace_authoring_custom_theme = (DatalabWorkspaceCustomTheme){
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
    runtime->workspace_authoring_custom_theme_active_slot = 0u;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        runtime->workspace_authoring_custom_theme_slots[i] = runtime->workspace_authoring_custom_theme;
        (void)snprintf(runtime->workspace_authoring_custom_theme_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "custom_%d",
                       i + 1);
    }
    runtime->input_root_from_cli = 0;
    runtime->selected_pack_path[0] = '\0';
    runtime->last_load_error[0] = '\0';
}

static int datalab_app_bootstrap_ctx(DatalabAppContext *ctx, int argc, char **argv) {
    DatalabAppRuntime *runtime = NULL;
    if (!ctx) {
        return 1;
    }
    runtime = ctx->runtime;
    if (!runtime) {
        return 1;
    }

    runtime->argv0 = (argv && argc > 0 && argv[0]) ? argv[0] : "datalab";
    runtime->pack_path = NULL;
    runtime->no_gui = 0;
    runtime->show_help = 0;
    runtime->input_root_from_cli = 0;
    runtime->selected_pack_path[0] = '\0';

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pack") == 0 && (i + 1) < argc) {
            runtime->pack_path = argv[++i];
        } else if (strcmp(argv[i], "--input-root") == 0 && (i + 1) < argc) {
            snprintf(runtime->input_root, sizeof(runtime->input_root), "%s", argv[++i]);
            runtime->input_root_from_cli = 1;
        } else if (strcmp(argv[i], "--no-gui") == 0) {
            runtime->no_gui = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            datalab_print_usage(runtime->argv0);
            runtime->show_help = 1;
            return 0;
        }
    }

    if (!datalab_app_transition_stage(ctx,
                                      DATALAB_APP_STAGE_INIT,
                                      DATALAB_APP_STAGE_BOOTSTRAPPED,
                                      "datalab_app_bootstrap",
                                      __func__)) {
        return 1;
    }
    ctx->ownership.bootstrap_owned = 1;
    return 0;
}

int datalab_app_bootstrap(int argc, char **argv, DatalabAppRuntime *runtime) {
    DatalabAppContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = runtime;
    ctx.stage = DATALAB_APP_STAGE_INIT;
    return datalab_app_bootstrap_ctx(&ctx, argc, argv);
}

static int datalab_app_config_load_ctx(DatalabAppContext *ctx) {
    int loaded_step = 0;
    uint8_t loaded_theme_preset = 0u;
    uint8_t loaded_custom_slot = 0u;
    int i = 0;
    DatalabWorkspaceCustomTheme loaded_custom_theme;
    DatalabWorkspaceCustomTheme loaded_custom_slots[DATALAB_CUSTOM_THEME_SLOT_COUNT];
    char loaded_custom_slot_names[DATALAB_CUSTOM_THEME_SLOT_COUNT][DATALAB_CUSTOM_THEME_NAME_CAP];
    char loaded_input_root[DATALAB_APP_PATH_CAP];
    DatalabAppRuntime *runtime = NULL;
    if (!ctx) {
        return 1;
    }
    runtime = ctx->runtime;
    if (!runtime) {
        return 1;
    }
    if (ctx->stage != DATALAB_APP_STAGE_BOOTSTRAPPED) {
        return 1;
    }
    runtime->text_zoom_step = 0;
    if (datalab_runtime_prefs_load_text_zoom_step(&loaded_step)) {
        runtime->text_zoom_step = loaded_step;
    }
    runtime->workspace_authoring_theme_preset_id =
        (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    loaded_custom_theme = runtime->workspace_authoring_custom_theme;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        loaded_custom_slots[i] = runtime->workspace_authoring_custom_theme_slots[i];
        (void)snprintf(loaded_custom_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "%s",
                       runtime->workspace_authoring_custom_theme_slot_names[i]);
    }
    if (datalab_runtime_prefs_load_theme_preset_id(&loaded_theme_preset)) {
        runtime->workspace_authoring_theme_preset_id = loaded_theme_preset;
    }
    if (datalab_runtime_prefs_load_custom_theme_slots(loaded_custom_slots, DATALAB_CUSTOM_THEME_SLOT_COUNT)) {
        for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
            runtime->workspace_authoring_custom_theme_slots[i] = loaded_custom_slots[i];
        }
    } else if (datalab_runtime_prefs_load_custom_theme(&loaded_custom_theme)) {
        runtime->workspace_authoring_custom_theme_slots[0] = loaded_custom_theme;
    }
    if (datalab_runtime_prefs_load_custom_theme_slot_names(loaded_custom_slot_names, DATALAB_CUSTOM_THEME_SLOT_COUNT)) {
        for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
            (void)snprintf(runtime->workspace_authoring_custom_theme_slot_names[i],
                           DATALAB_CUSTOM_THEME_NAME_CAP,
                           "%s",
                           loaded_custom_slot_names[i]);
        }
    }
    if (datalab_runtime_prefs_load_custom_theme_active_slot(&loaded_custom_slot)) {
        runtime->workspace_authoring_custom_theme_active_slot = loaded_custom_slot;
    }
    if (runtime->workspace_authoring_custom_theme_active_slot >= DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        runtime->workspace_authoring_custom_theme_active_slot = 0u;
    }
    runtime->workspace_authoring_custom_theme =
        runtime->workspace_authoring_custom_theme_slots[runtime->workspace_authoring_custom_theme_active_slot];
    if (runtime->input_root[0] == '\0') {
        snprintf(runtime->input_root, sizeof(runtime->input_root), "%s", k_datalab_default_input_root);
    }
    loaded_input_root[0] = '\0';
    if (!runtime->input_root_from_cli &&
        datalab_runtime_prefs_load_input_root(loaded_input_root, sizeof(loaded_input_root)) &&
        loaded_input_root[0] != '\0') {
        snprintf(runtime->input_root, sizeof(runtime->input_root), "%s", loaded_input_root);
    }
    if (!datalab_app_transition_stage(ctx,
                                      DATALAB_APP_STAGE_BOOTSTRAPPED,
                                      DATALAB_APP_STAGE_CONFIG_LOADED,
                                      "datalab_app_config_load",
                                      __func__)) {
        return 1;
    }
    ctx->ownership.config_owned = 1;
    return 0;
}

int datalab_app_config_load(DatalabAppRuntime *runtime) {
    DatalabAppContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = runtime;
    ctx.stage = DATALAB_APP_STAGE_BOOTSTRAPPED;
    return datalab_app_config_load_ctx(&ctx);
}

static int datalab_app_state_seed_ctx(DatalabAppContext *ctx) {
    DatalabAppRuntime *runtime = NULL;
    if (!ctx) {
        return 1;
    }
    runtime = ctx->runtime;
    if (!runtime) {
        return 1;
    }
    if (ctx->stage != DATALAB_APP_STAGE_CONFIG_LOADED) {
        return 1;
    }

    if (!runtime->pack_path && runtime->selected_pack_path[0] != '\0') {
        runtime->pack_path = runtime->selected_pack_path;
    }

    if (!runtime->pack_path && runtime->no_gui) {
        datalab_print_usage(runtime->argv0 ? runtime->argv0 : "datalab");
        return 1;
    }

    if (!runtime->pack_path) {
        if (!datalab_app_transition_stage(ctx,
                                          DATALAB_APP_STAGE_CONFIG_LOADED,
                                          DATALAB_APP_STAGE_STATE_SEEDED,
                                          "datalab_app_state_seed",
                                          __func__)) {
            return 1;
        }
        ctx->ownership.state_seed_owned = 1;
        return 0;
    }

    {
        int load_rc = datalab_runtime_load_frame(runtime);
        if (load_rc != 0) {
            return load_rc;
        }
        datalab_runtime_print_loaded_frame_summary(runtime);
        load_rc = datalab_runtime_validate_loaded_physics_dataset(runtime);
        if (load_rc != 0) {
            return load_rc;
        }
    }

    if (!datalab_app_transition_stage(ctx,
                                      DATALAB_APP_STAGE_CONFIG_LOADED,
                                      DATALAB_APP_STAGE_STATE_SEEDED,
                                      "datalab_app_state_seed",
                                      __func__)) {
        return 1;
    }
    ctx->ownership.state_seed_owned = 1;
    return 0;
}

int datalab_app_state_seed(DatalabAppRuntime *runtime) {
    DatalabAppContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = runtime;
    ctx.stage = DATALAB_APP_STAGE_CONFIG_LOADED;
    return datalab_app_state_seed_ctx(&ctx);
}

int datalab_app_subsystems_init(DatalabAppRuntime *runtime, DatalabAppState *app_state) {
    int i = 0;
    if (!runtime) {
        return 1;
    }
    if (runtime->no_gui) {
        return 0;
    }
    if (!app_state) {
        return 1;
    }

    datalab_app_state_init(app_state, runtime->pack_path, runtime->frame.profile);
    app_state->text_zoom_step = runtime->text_zoom_step;
    app_state->workspace_authoring_theme_preset_id = runtime->workspace_authoring_theme_preset_id;
    app_state->workspace_authoring_custom_theme_active_slot =
        runtime->workspace_authoring_custom_theme_active_slot;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        app_state->workspace_authoring_custom_theme_slots[i] =
            runtime->workspace_authoring_custom_theme_slots[i];
        (void)snprintf(app_state->workspace_authoring_custom_theme_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "%s",
                       runtime->workspace_authoring_custom_theme_slot_names[i]);
    }
    app_state->workspace_authoring_custom_theme = runtime->workspace_authoring_custom_theme;
    app_state->workspace_authoring_entry_text_zoom_step = app_state->text_zoom_step;
    app_state->workspace_authoring_entry_theme_preset_id = app_state->workspace_authoring_theme_preset_id;
    app_state->workspace_authoring_entry_custom_theme_active_slot =
        app_state->workspace_authoring_custom_theme_active_slot;
    for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        app_state->workspace_authoring_entry_custom_theme_slots[i] =
            app_state->workspace_authoring_custom_theme_slots[i];
        (void)snprintf(app_state->workspace_authoring_entry_custom_theme_slot_names[i],
                       DATALAB_CUSTOM_THEME_NAME_CAP,
                       "%s",
                       app_state->workspace_authoring_custom_theme_slot_names[i]);
    }
    snprintf(app_state->input_root, sizeof(app_state->input_root), "%s", runtime->input_root);
    app_state->open_picker_requested = 0;
    return 0;
}

int datalab_runtime_start(DatalabAppRuntime *runtime, DatalabAppState *app_state) {
    CoreResult run_r;
    int picker_enter_authoring = 0;
    if (!runtime) {
        return 1;
    }
    if (runtime->no_gui) {
        return 0;
    }
    for (;;) {
        if (!runtime->frame_loaded) {
            if (!runtime->pack_path || runtime->pack_path[0] == '\0') {
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
                    return 4;
                }
                runtime->last_load_error[0] = '\0';
                runtime->pack_path = runtime->selected_pack_path;
            }
            if (!runtime->pack_path || runtime->pack_path[0] == '\0') {
                datalab_runtime_prefs_save_input_root(runtime->input_root);
                return 0;
            }
            {
                int load_rc = datalab_runtime_load_frame(runtime);
                if (load_rc != 0) {
                    snprintf(runtime->last_load_error,
                             sizeof(runtime->last_load_error),
                             "load failed: %s",
                             runtime->last_load_error[0] ? runtime->last_load_error : "unsupported or invalid pack");
                    runtime->pack_path = NULL;
                    runtime->selected_pack_path[0] = '\0';
                    datalab_frame_free(&runtime->frame);
                    datalab_frame_init(&runtime->frame);
                    runtime->frame_loaded = 0;
                    continue;
                }
            }
            if (!runtime->frame_loaded) {
                return 2;
            }
            if (app_state) {
                int i = 0;
                datalab_app_state_init(app_state, runtime->pack_path, runtime->frame.profile);
                app_state->text_zoom_step = runtime->text_zoom_step;
                app_state->workspace_authoring_theme_preset_id = runtime->workspace_authoring_theme_preset_id;
                app_state->workspace_authoring_custom_theme_active_slot =
                    runtime->workspace_authoring_custom_theme_active_slot;
                for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
                    app_state->workspace_authoring_custom_theme_slots[i] =
                        runtime->workspace_authoring_custom_theme_slots[i];
                    (void)snprintf(app_state->workspace_authoring_custom_theme_slot_names[i],
                                   DATALAB_CUSTOM_THEME_NAME_CAP,
                                   "%s",
                                   runtime->workspace_authoring_custom_theme_slot_names[i]);
                }
                app_state->workspace_authoring_custom_theme = runtime->workspace_authoring_custom_theme;
                app_state->workspace_authoring_entry_text_zoom_step = app_state->text_zoom_step;
                app_state->workspace_authoring_entry_theme_preset_id =
                    app_state->workspace_authoring_theme_preset_id;
                app_state->workspace_authoring_entry_custom_theme_active_slot =
                    app_state->workspace_authoring_custom_theme_active_slot;
                for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
                    app_state->workspace_authoring_entry_custom_theme_slots[i] =
                        app_state->workspace_authoring_custom_theme_slots[i];
                    (void)snprintf(app_state->workspace_authoring_entry_custom_theme_slot_names[i],
                                   DATALAB_CUSTOM_THEME_NAME_CAP,
                                   "%s",
                                   app_state->workspace_authoring_custom_theme_slot_names[i]);
                }
                snprintf(app_state->input_root, sizeof(app_state->input_root), "%s", runtime->input_root);
                app_state->open_picker_requested = 0;
                app_state->panel_rescan_requested = 1;
                if (picker_enter_authoring) {
                    app_state->workspace_authoring_stub_active = 1;
                    app_state->workspace_authoring_overlay_mode =
                        DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
                    app_state->workspace_authoring_pending_stub = 0u;
                    app_state->workspace_authoring_entry_theme_preset_id =
                        app_state->workspace_authoring_theme_preset_id;
                    app_state->workspace_authoring_entry_count += 1u;
                    picker_enter_authoring = 0;
                }
            }
        }

        run_r = datalab_render_run(&runtime->frame, app_state);
        if (app_state) {
            int i = 0;
            runtime->workspace_authoring_theme_preset_id = app_state->workspace_authoring_theme_preset_id;
            runtime->workspace_authoring_custom_theme_active_slot =
                app_state->workspace_authoring_custom_theme_active_slot;
            runtime->workspace_authoring_custom_theme = app_state->workspace_authoring_custom_theme;
            for (i = 0; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
                runtime->workspace_authoring_custom_theme_slots[i] =
                    app_state->workspace_authoring_custom_theme_slots[i];
                (void)snprintf(runtime->workspace_authoring_custom_theme_slot_names[i],
                               DATALAB_CUSTOM_THEME_NAME_CAP,
                               "%s",
                               app_state->workspace_authoring_custom_theme_slot_names[i]);
            }
            runtime->text_zoom_step = app_state->text_zoom_step;
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
        if (run_r.code != CORE_OK) {
            fprintf(stderr, "datalab: render failed: %s\n", run_r.message);
            return 4;
        }
        if (app_state && app_state->panel_requested_pack_path[0] != '\0') {
            snprintf(runtime->selected_pack_path,
                     sizeof(runtime->selected_pack_path),
                     "%s",
                     app_state->panel_requested_pack_path);
            runtime->pack_path = runtime->selected_pack_path;
            app_state->panel_requested_pack_path[0] = '\0';
        } else if (app_state && app_state->open_picker_requested) {
            runtime->pack_path = NULL;
            app_state->open_picker_requested = 0;
        } else {
            return 0;
        }
        if (runtime->frame_loaded) {
            datalab_frame_free(&runtime->frame);
            datalab_frame_init(&runtime->frame);
            runtime->frame_loaded = 0;
        }
    }
}

static int datalab_default_runtime_dispatch(const DatalabDispatchRequest *request,
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

static int datalab_app_run_loop_ctx(DatalabAppContext *ctx) {
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

static void datalab_app_shutdown_ctx(DatalabAppContext *ctx) {
    DatalabAppRuntime *runtime = NULL;
    if (!ctx || !ctx->runtime) {
        return;
    }
    runtime = ctx->runtime;
    if (runtime->frame_loaded) {
        datalab_frame_free(&runtime->frame);
        runtime->frame_loaded = 0;
    }
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

int datalab_app_main(int argc, char **argv) {
    DatalabAppRuntime runtime;
    DatalabAppContext ctx;
    int rc = 0;

    datalab_app_runtime_init(&runtime);
    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = &runtime;
    ctx.stage = DATALAB_APP_STAGE_INIT;
    ctx.runtime_dispatch = datalab_default_runtime_dispatch;
    ctx.wrapper_error = DATALAB_WRAPPER_ERROR_NONE;
    ctx.exit_code = 1;

    rc = datalab_app_bootstrap_ctx(&ctx, argc, argv);
    if (rc != 0) {
        ctx.wrapper_error = DATALAB_WRAPPER_ERROR_BOOTSTRAP_FAILED;
        datalab_log_wrapper_error(__func__, ctx.wrapper_error, ctx.stage, rc, "bootstrap failed");
        goto done;
    }

    if (runtime.show_help) {
        rc = 0;
        goto done;
    }

    rc = datalab_app_config_load_ctx(&ctx);
    if (rc != 0) {
        ctx.wrapper_error = DATALAB_WRAPPER_ERROR_CONFIG_LOAD_FAILED;
        datalab_log_wrapper_error(__func__, ctx.wrapper_error, ctx.stage, rc, "config load failed");
        goto done;
    }

    rc = datalab_app_state_seed_ctx(&ctx);
    if (rc != 0) {
        ctx.wrapper_error = DATALAB_WRAPPER_ERROR_STATE_SEED_FAILED;
        datalab_log_wrapper_error(__func__, ctx.wrapper_error, ctx.stage, rc, "state seed failed");
        goto done;
    }

    rc = datalab_app_run_loop_ctx(&ctx);
    if (rc != 0 &&
        ctx.wrapper_error == DATALAB_WRAPPER_ERROR_NONE) {
        ctx.wrapper_error = DATALAB_WRAPPER_ERROR_STAGE_TRANSITION_FAILED;
        datalab_log_wrapper_error(__func__, ctx.wrapper_error, ctx.stage, rc, "run loop stage failure");
    }

done:
    datalab_app_shutdown_ctx(&ctx);
    ctx.exit_code = rc;
    fprintf(stderr,
            "datalab: wrapper exit stage=%d exit_code=%d dispatch_count=%u dispatch_ok=%d last_dispatch_exit=%d wrapper_error=%d\n",
            (int)ctx.stage,
            ctx.exit_code,
            ctx.dispatch_summary.dispatch_count,
            ctx.dispatch_summary.dispatch_succeeded,
            ctx.dispatch_summary.last_dispatch_exit_code,
            (int)ctx.wrapper_error);
    return ctx.exit_code;
}

int datalab_app_main_legacy(int argc, char **argv) {
    const char *pack_path = NULL;
    int no_gui = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pack") == 0 && (i + 1) < argc) {
            pack_path = argv[++i];
        } else if (strcmp(argv[i], "--no-gui") == 0) {
            no_gui = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            datalab_print_usage(argv[0]);
            return 0;
        }
    }

    if (!pack_path) {
        datalab_print_usage(argv[0]);
        return 1;
    }

    DatalabFrame frame;
    CoreResult load_r = datalab_load_pack(pack_path, &frame);
    if (load_r.code != CORE_OK) {
        fprintf(stderr, "datalab: failed to load pack: %s\n", load_r.message);
        return 2;
    }

    datalab_print_frame_summary(pack_path, &frame);

    if (frame.profile == DATALAB_PROFILE_PHYSICS) {
        CoreDataset dataset;
        CoreResult ds_r = datalab_build_dataset_from_frame(&frame, &dataset);
        if (ds_r.code != CORE_OK) {
            fprintf(stderr, "datalab: dataset build failed: %s\n", ds_r.message);
            datalab_frame_free(&frame);
            return 3;
        }
        core_dataset_free(&dataset);
    }

    if (!no_gui) {
        DatalabAppState app_state;
        datalab_app_state_init(&app_state, pack_path, frame.profile);

        CoreResult run_r = datalab_render_run(&frame, &app_state);
        datalab_frame_free(&frame);
        if (run_r.code != CORE_OK) {
            fprintf(stderr, "datalab: render failed: %s\n", run_r.message);
            return 4;
        }
    } else {
        datalab_frame_free(&frame);
    }

    return 0;
}
