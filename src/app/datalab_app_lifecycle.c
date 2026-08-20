#include "app/datalab_app_internal.h"
#include "app/datalab_runtime_prefs.h"

#include <sys/stat.h>

#include "data/input_file_loader.h"

#include <stdio.h>
#include <string.h>

#include "app/datalab_runtime_pack.h"

static const char *k_datalab_default_input_root = "data/import";

static int datalab_last_opened_input_is_in_restored_catalog(const DatalabAppRuntime *runtime,
                                                            const char *input_path) {
    const char *last_slash = NULL;
    size_t root_len = 0u;
    if (!runtime || !input_path || input_path[0] == '\0' || !datalab_input_file_is_supported(input_path)) {
        return 0;
    }
    last_slash = strrchr(input_path, '/');
    if (!last_slash || last_slash == input_path || last_slash[1] == '\0') {
        return 0;
    }
    root_len = (size_t)(last_slash - input_path);
    return strlen(runtime->input_root) == root_len &&
           strncmp(runtime->input_root, input_path, root_len) == 0 &&
           datalab_input_catalog_file_is_current(&runtime->input_catalog,
                                                 runtime->input_root,
                                                 last_slash + 1);
}

int datalab_app_transition_stage(DatalabAppContext *ctx,
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

void datalab_print_usage(const char *argv0) {
    printf("usage: %s [--pack /path/to/frame.pack|frame.bmp|frame.png] [--input-root /path/to/folder] [--no-gui] [--visual-artifact /path/to/frame.bmp] [--w5-acceptance /new/temp/output-dir]\n", argv0);
}

void datalab_app_runtime_init(DatalabAppRuntime *runtime) {
    int i = 0;
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    datalab_frame_init(&runtime->frame);
    datalab_input_catalog_init(&runtime->input_catalog);
    datalab_async_decode_init(&runtime->async_decode);
    datalab_focus_window_init(&runtime->focus_window);
    runtime->image_residency = (DatalabImageResidency *)core_alloc(sizeof(*runtime->image_residency));
    if (runtime->image_residency) {
        datalab_image_residency_init(runtime->image_residency);
    }
    snprintf(runtime->input_root, sizeof(runtime->input_root), "%s", k_datalab_default_input_root);
    runtime->recent_input_root_count = 0u;
    runtime->workspace_authoring_theme_preset_id =
        (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_MIDNIGHT_CONTRAST;
    runtime->workspace_authoring_profile_surface_ratio = 0.72f;
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
    runtime->reopened_last_input_file = 0;
    runtime->playback_active = 0;
    runtime->playback_mode = DATALAB_PLAYBACK_MODE_LOOP;
    runtime->playback_direction = 1;
    runtime->playback_speed_index = DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT;
    runtime->playback_interval_ms = DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT;
    runtime->session_hud_collapsed = 0;
    runtime->sampling_mode = DATALAB_SAMPLING_MODE_DEFAULT;
    datalab_raster_viewport_state_init(&runtime->raster_viewport);
    datalab_viewer_session_init(&runtime->viewer_session);
    runtime->viewer_session_restore_pending = 0;
    runtime->selected_pack_path[0] = '\0';
    runtime->visual_artifact_path[0] = '\0';
    runtime->last_load_error[0] = '\0';
}

int datalab_app_bootstrap_ctx(DatalabAppContext *ctx, int argc, char **argv) {
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
    runtime->visual_artifact_path[0] = '\0';

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pack") == 0 && (i + 1) < argc) {
            runtime->pack_path = argv[++i];
        } else if (strcmp(argv[i], "--input-root") == 0 && (i + 1) < argc) {
            snprintf(runtime->input_root, sizeof(runtime->input_root), "%s", argv[++i]);
            runtime->input_root_from_cli = 1;
        } else if (strcmp(argv[i], "--no-gui") == 0) {
            runtime->no_gui = 1;
        } else if (strcmp(argv[i], "--visual-artifact") == 0 && (i + 1) < argc) {
            snprintf(runtime->visual_artifact_path, sizeof(runtime->visual_artifact_path), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            datalab_print_usage(runtime->argv0);
            runtime->show_help = 1;
            return 0;
        }
    }

    if (runtime->visual_artifact_path[0] != '\0' && runtime->no_gui) {
        datalab_print_usage(runtime->argv0);
        return 1;
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

int datalab_app_config_load_ctx(DatalabAppContext *ctx) {
    int loaded_step = 0;
    uint8_t loaded_theme_preset = 0u;
    uint8_t loaded_custom_slot = 0u;
    float loaded_profile_surface_ratio = 0.0f;
    int i = 0;
    DatalabWorkspaceCustomTheme loaded_custom_theme;
    DatalabWorkspaceCustomTheme loaded_custom_slots[DATALAB_CUSTOM_THEME_SLOT_COUNT];
    char loaded_custom_slot_names[DATALAB_CUSTOM_THEME_SLOT_COUNT][DATALAB_CUSTOM_THEME_NAME_CAP];
    char loaded_input_root[DATALAB_APP_PATH_CAP];
    char loaded_last_opened_input_file[DATALAB_APP_PATH_CAP];
    DatalabStartupSurface startup_surface = DATALAB_STARTUP_SURFACE_PICKER;
    char loaded_recent_input_roots[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP];
    size_t loaded_recent_input_root_count = 0u;
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
    if (datalab_runtime_prefs_load_workspace_authoring_profile_surface_ratio(&loaded_profile_surface_ratio)) {
        runtime->workspace_authoring_profile_surface_ratio = loaded_profile_surface_ratio;
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
    if (datalab_runtime_prefs_load_recent_input_roots(loaded_recent_input_roots,
                                                      DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                                      &loaded_recent_input_root_count)) {
        size_t recent_idx = 0u;
        runtime->recent_input_root_count = loaded_recent_input_root_count;
        for (recent_idx = 0u; recent_idx < loaded_recent_input_root_count; ++recent_idx) {
            snprintf(runtime->recent_input_roots[recent_idx],
                     DATALAB_APP_PATH_CAP,
                     "%s",
                     loaded_recent_input_roots[recent_idx]);
        }
    }
    (void)datalab_input_root_select_recent(runtime->input_root,
                                           sizeof(runtime->input_root),
                                           runtime->recent_input_roots,
                                           &runtime->recent_input_root_count,
                                           DATALAB_RECENT_INPUT_ROOT_LIMIT,
                                           runtime->input_root);
    (void)datalab_runtime_prefs_load_startup_surface(&startup_surface);
    /* Refresh the retained catalog before any viewer restoration. The saved
     * root is its own preference and must not be re-derived from a file path. */
    if (!runtime->no_gui) {
        (void)datalab_input_catalog_refresh(&runtime->input_catalog,
                                            runtime->input_root,
                                            DATALAB_INPUT_CATALOG_REFRESH_INITIAL);
    }
    loaded_last_opened_input_file[0] = '\0';
    if (startup_surface == DATALAB_STARTUP_SURFACE_VIEWER &&
        !runtime->no_gui && !runtime->input_root_from_cli && !runtime->pack_path) {
        const int session_loaded = datalab_viewer_session_load(&runtime->viewer_session);
        const char *restore_path = NULL;
        if (session_loaded && runtime->viewer_session.selected_path_present) {
            restore_path = runtime->viewer_session.selected_path;
        } else if ((!session_loaded || !runtime->viewer_session.selected_path_present) &&
                   datalab_runtime_prefs_load_last_opened_input_file(loaded_last_opened_input_file,
                                                                      sizeof(loaded_last_opened_input_file))) {
            restore_path = loaded_last_opened_input_file;
        }
        if (restore_path && datalab_last_opened_input_is_in_restored_catalog(runtime, restore_path)) {
            snprintf(runtime->selected_pack_path,
                     sizeof(runtime->selected_pack_path),
                     "%s",
                     restore_path);
            runtime->pack_path = runtime->selected_pack_path;
            runtime->reopened_last_input_file = 1;
            runtime->viewer_session_restore_pending = session_loaded;
        }
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

int datalab_app_state_seed_ctx(DatalabAppContext *ctx) {
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
    if (!runtime->pack_path && runtime->visual_artifact_path[0] != '\0') {
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
            if (runtime->reopened_last_input_file && !runtime->no_gui) {
                runtime->reopened_last_input_file = 0;
                runtime->pack_path = NULL;
                runtime->selected_pack_path[0] = '\0';
                runtime->last_load_error[0] = '\0';
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
