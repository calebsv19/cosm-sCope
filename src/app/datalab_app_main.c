#include "datalab/datalab_app_main.h"

#include <stdio.h>
#include <string.h>

#include "app/datalab_app_internal.h"
#include "core_data.h"
#include "data/dataset_builders.h"
#include "data/input_file_loader.h"
#include "render/render_view.h"

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
    CoreResult load_r = datalab_load_input_file(pack_path, &frame);
    if (load_r.code != CORE_OK) {
        fprintf(stderr, "datalab: failed to load input file: %s\n", load_r.message);
        return 2;
    }

    if (frame.profile == DATALAB_PROFILE_IMAGE) {
        printf("input=%s\n", pack_path);
        printf("  profile=image raster=%ux%u\n", frame.width, frame.height);
    } else {
        datalab_print_frame_summary(pack_path, &frame);
    }

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
        DatalabRenderSession *render_session = NULL;
        datalab_app_state_init(&app_state, pack_path, frame.profile);
        CoreResult run_r = datalab_render_session_open(&render_session);
        if (run_r.code == CORE_OK) {
            run_r = datalab_render_run_with_session(render_session, &frame, &app_state);
        }
        datalab_render_session_close(render_session);
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
