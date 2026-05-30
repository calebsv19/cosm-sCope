#ifndef DATALAB_APP_INTERNAL_H
#define DATALAB_APP_INTERNAL_H

#include "datalab/datalab_app_main.h"

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

typedef struct DatalabAppContext {
    DatalabAppRuntime *runtime;
    DatalabAppStage stage;
    int (*runtime_dispatch)(const DatalabDispatchRequest *request, DatalabDispatchOutcome *outcome);
    DatalabDispatchSummary dispatch_summary;
    DatalabLifecycleOwnership ownership;
    DatalabWrapperError wrapper_error;
    int exit_code;
} DatalabAppContext;

int datalab_app_transition_stage(DatalabAppContext *ctx,
                                 DatalabAppStage expected,
                                 DatalabAppStage next,
                                 const char *stage_name,
                                 const char *fn_name);
int datalab_default_runtime_dispatch(const DatalabDispatchRequest *request,
                                     DatalabDispatchOutcome *outcome);
void datalab_log_wrapper_error(const char *fn_name,
                               DatalabWrapperError wrapper_error,
                               DatalabAppStage stage,
                               int exit_code,
                               const char *detail);
void datalab_print_usage(const char *argv0);
int datalab_app_bootstrap_ctx(DatalabAppContext *ctx, int argc, char **argv);
int datalab_app_config_load_ctx(DatalabAppContext *ctx);
int datalab_app_state_seed_ctx(DatalabAppContext *ctx);
int datalab_app_run_loop_ctx(DatalabAppContext *ctx);
void datalab_app_shutdown_ctx(DatalabAppContext *ctx);

void datalab_runtime_copy_to_app_state(const DatalabAppRuntime *runtime,
                                       DatalabAppState *app_state,
                                       int panel_rescan_requested);
void datalab_runtime_copy_from_app_state(DatalabAppRuntime *runtime,
                                         const DatalabAppState *app_state);

#endif
