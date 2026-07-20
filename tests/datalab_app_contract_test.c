#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <png.h>
#include <SDL2/SDL.h>

#include "app/datalab_app_internal.h"
#include "app/datalab_runtime_prefs.h"
#include "datalab/datalab_app_main.h"
#include "data/input_file_loader.h"
#include "data/pack_inspector.h"
#include "render/render_view.h"
#include "render/render_view_internal.h"
#include "core_pack.h"

#ifndef DATALAB_TEST_DEFAULT_PACK
#define DATALAB_TEST_DEFAULT_PACK ""
#endif

static int g_wrapper_lifecycle_dispatch_calls = 0;
static const DatalabAppRuntime *g_wrapper_lifecycle_runtime = NULL;

static int datalab_test_wrapper_lifecycle_dispatch(const DatalabDispatchRequest *request,
                                                   DatalabDispatchOutcome *outcome) {
    if (!request || !request->runtime || !outcome) {
        return 1;
    }
    ++g_wrapper_lifecycle_dispatch_calls;
    g_wrapper_lifecycle_runtime = request->runtime;
    memset(outcome, 0, sizeof(*outcome));
    outcome->exit_code = 0;
    outcome->dispatched = 1;
    outcome->runtime_started = 1;
    return 0;
}

static int datalab_test_mkdir_if_needed(const char *path) {
    if (!path || path[0] == '\0') {
        return 0;
    }
    if (mkdir(path, 0777) == 0) {
        return 1;
    }
    return errno == EEXIST;
}

static int datalab_test_write_text_file(const char *path, const char *text) {
    FILE *fp = NULL;
    if (!path || !text) {
        return 0;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    if (fputs(text, fp) < 0) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static int datalab_test_write_tiny_bmp(const char *path) {
    SDL_Surface *surface = NULL;
    uint32_t *pixels = NULL;
    int pitch_pixels = 0;
    if (!path) {
        return 0;
    }
    surface = SDL_CreateRGBSurfaceWithFormat(0, 2, 2, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return 0;
    }
    pixels = (uint32_t *)surface->pixels;
    pitch_pixels = surface->pitch / (int)sizeof(uint32_t);
    pixels[0] = SDL_MapRGBA(surface->format, 255u, 0u, 0u, 255u);
    pixels[1] = SDL_MapRGBA(surface->format, 0u, 255u, 0u, 255u);
    pixels[pitch_pixels] = SDL_MapRGBA(surface->format, 0u, 0u, 255u, 255u);
    pixels[pitch_pixels + 1] = SDL_MapRGBA(surface->format, 255u, 255u, 0u, 255u);
    if (SDL_SaveBMP(surface, path) != 0) {
        SDL_FreeSurface(surface);
        return 0;
    }
    SDL_FreeSurface(surface);
    return 1;
}

static int datalab_test_write_tiny_png(const char *path) {
    static const uint8_t pixels[] = {
        255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u,
        0u, 0u, 255u, 255u, 255u, 255u, 0u, 255u
    };
    png_bytep rows[2];
    FILE *fp = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    int ok = 0;
    if (!path) {
        return 0;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        goto cleanup;
    }
    rows[0] = (png_bytep)pixels;
    rows[1] = (png_bytep)(pixels + 8u);
    png_init_io(png, fp);
    png_set_IHDR(png, info, 2u, 2u, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    png_write_image(png, rows);
    png_write_end(png, info);
    ok = 1;
cleanup:
    if (png || info) {
        png_destroy_write_struct(&png, &info);
    }
    fclose(fp);
    return ok;
}

static int datalab_test_write_generic_pack(const char *path) {
    CorePackWriter writer = {0};
    static const char header[] = "inspection fixture";
    static const uint8_t payload[] = {1u, 2u, 3u, 4u};
    if (!path || core_pack_writer_open(path, &writer).code != CORE_OK) {
        return 0;
    }
    if (core_pack_writer_add_chunk(&writer, "TEST", header, sizeof(header)).code != CORE_OK ||
        core_pack_writer_add_chunk(&writer, "DATA", payload, sizeof(payload)).code != CORE_OK ||
        core_pack_writer_close(&writer).code != CORE_OK) {
        return 0;
    }
    return 1;
}

static int datalab_test_make_temp_dir(char *out_dir, size_t out_cap) {
    char template_path[PATH_MAX];
    char *made = NULL;
    if (!out_dir || out_cap == 0u) {
        return 0;
    }
    snprintf(template_path, sizeof(template_path), "/tmp/datalab_contract_XXXXXX");
    made = mkdtemp(template_path);
    if (!made) {
        return 0;
    }
    snprintf(out_dir, out_cap, "%s", made);
    return 1;
}

static int datalab_test_enter_temp_runtime_root(const char *temp_dir, char *previous_cwd, size_t cwd_cap) {
    char data_dir[PATH_MAX];
    char runtime_dir[PATH_MAX];
    if (!temp_dir || !previous_cwd || cwd_cap == 0u) {
        return 0;
    }
    if (!getcwd(previous_cwd, cwd_cap)) {
        return 0;
    }
    snprintf(data_dir, sizeof(data_dir), "%s/data", temp_dir);
    snprintf(runtime_dir, sizeof(runtime_dir), "%s/data/runtime", temp_dir);
    if (!datalab_test_mkdir_if_needed(temp_dir) ||
        !datalab_test_mkdir_if_needed(data_dir) ||
        !datalab_test_mkdir_if_needed(runtime_dir)) {
        return 0;
    }
    if (chdir(temp_dir) != 0) {
        return 0;
    }
    return 1;
}

static void datalab_test_restore_cwd(const char *cwd) {
    if (cwd && cwd[0] != '\0') {
        (void)chdir(cwd);
    }
}

static int datalab_test_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "contract: %s\n", message ? message : "assertion failed");
        return 0;
    }
    return 1;
}

static int test_headless_requires_pack(void) {
    DatalabAppRuntime runtime;
    char *argv[] = { (char *)"datalab", (char *)"--no-gui" };
    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(datalab_app_bootstrap(2, argv, &runtime) == 0, "bootstrap failed for no-gui case")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for no-gui case")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) != 0, "headless launch without pack should fail")) {
        return 0;
    }
    return 1;
}

static int test_valid_headless_pack_state_seed(const char *default_pack) {
    DatalabAppRuntime runtime;
    char *argv[] = { (char *)"datalab", (char *)"--pack", (char *)default_pack, (char *)"--no-gui" };
    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(default_pack && default_pack[0] != '\0', "default pack path missing")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_bootstrap(4, argv, &runtime) == 0, "bootstrap failed for valid pack")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for valid pack")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) == 0, "state seed failed for valid pack")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    if (!datalab_test_assert(runtime.frame_loaded == 1, "valid pack should load a frame")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    datalab_app_shutdown(&runtime);
    return 1;
}

static int test_selected_pack_path_fallback(const char *default_pack) {
    DatalabAppRuntime runtime;
    char *argv[] = { (char *)"datalab", (char *)"--no-gui" };
    datalab_app_runtime_init(&runtime);
    snprintf(runtime.selected_pack_path, sizeof(runtime.selected_pack_path), "%s", default_pack);
    if (!datalab_test_assert(datalab_app_bootstrap(2, argv, &runtime) == 0, "bootstrap failed for selected-pack fallback")) {
        return 0;
    }
    snprintf(runtime.selected_pack_path, sizeof(runtime.selected_pack_path), "%s", default_pack);
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for selected-pack fallback")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) == 0, "state seed failed for selected-pack fallback")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    if (!datalab_test_assert(runtime.pack_path == runtime.selected_pack_path, "selected-pack fallback should bind pack_path to runtime storage")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    if (!datalab_test_assert(runtime.frame_loaded == 1, "selected-pack fallback should load a frame")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    datalab_app_shutdown(&runtime);
    return 1;
}

static int test_generated_bmp_load_path_and_state_seed(const char *temp_dir) {
    DatalabFrame frame = {0};
    DatalabAppRuntime runtime;
    CoreResult result;
    char bmp_path[PATH_MAX];
    char *argv[] = { (char *)"datalab", (char *)"--pack", bmp_path, (char *)"--no-gui" };

    snprintf(bmp_path, sizeof(bmp_path), "%s/tiny_fixture.bmp", temp_dir);
    if (!datalab_test_write_tiny_bmp(bmp_path)) {
        fprintf(stderr, "contract: failed to create bmp fixture: %s\n", SDL_GetError());
        return 0;
    }

    result = datalab_load_input_file(bmp_path, &frame);
    if (!datalab_test_assert(result.code == CORE_OK, "generated bmp should load through input loader") ||
        !datalab_test_assert(frame.profile == DATALAB_PROFILE_IMAGE, "generated bmp should seed image profile") ||
        !datalab_test_assert(frame.width == 2u && frame.height == 2u, "generated bmp dimensions should load") ||
        !datalab_test_assert(frame.logical_width == 2u && frame.logical_height == 2u,
                             "generated bmp logical dimensions should match raster") ||
        !datalab_test_assert(frame.drawing_rgba != NULL, "generated bmp should produce rgba pixels")) {
        datalab_frame_free(&frame);
        return 0;
    }
    datalab_frame_free(&frame);

    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(datalab_app_bootstrap(4, argv, &runtime) == 0,
                             "bootstrap failed for generated bmp")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                             "config load failed for generated bmp")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) == 0,
                             "state seed failed for generated bmp")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    if (!datalab_test_assert(runtime.frame_loaded == 1, "generated bmp should load a runtime frame") ||
        !datalab_test_assert(runtime.frame.profile == DATALAB_PROFILE_IMAGE,
                             "generated bmp runtime frame should use image profile") ||
        !datalab_test_assert(runtime.frame.width == 2u && runtime.frame.height == 2u,
                             "generated bmp runtime dimensions should load") ||
        !datalab_test_assert(runtime.frame.drawing_rgba != NULL,
                             "generated bmp runtime frame should own rgba pixels")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    datalab_app_shutdown(&runtime);
    return 1;
}

static int test_generated_png_load_path(const char *temp_dir) {
    DatalabFrame frame = {0};
    CoreResult result;
    char png_path[PATH_MAX];
    snprintf(png_path, sizeof(png_path), "%s/tiny_fixture.png", temp_dir);
    if (!datalab_test_write_tiny_png(png_path)) {
        fprintf(stderr, "contract: failed to create png fixture\n");
        return 0;
    }
    result = datalab_load_input_file(png_path, &frame);
    if (!datalab_test_assert(result.code == CORE_OK, "generated png should load through input loader") ||
        !datalab_test_assert(datalab_input_file_is_png(png_path), "png extension should be supported") ||
        !datalab_test_assert(frame.profile == DATALAB_PROFILE_IMAGE, "generated png should seed image profile") ||
        !datalab_test_assert(frame.width == 2u && frame.height == 2u, "generated png dimensions should load") ||
        !datalab_test_assert(frame.drawing_rgba != NULL, "generated png should produce rgba pixels")) {
        datalab_frame_free(&frame);
        return 0;
    }
    datalab_frame_free(&frame);
    return 1;
}

static int test_generic_pack_inspection(const char *temp_dir) {
    DatalabPackInspection inspection = {0};
    CoreResult result;
    char path[PATH_MAX];
    char summary[128];
    char chunk[64];
    snprintf(path, sizeof(path), "%s/generic_fixture.pack", temp_dir);
    if (!datalab_test_write_generic_pack(path)) {
        fprintf(stderr, "contract: failed to create generic pack fixture\n");
        return 0;
    }
    result = datalab_inspect_pack(path, &inspection);
    datalab_pack_inspection_format_summary(&inspection, summary, sizeof(summary));
    datalab_pack_inspection_format_chunk(&inspection, 1u, chunk, sizeof(chunk));
    return datalab_test_assert(result.code == CORE_OK, "generic pack should inspect") &&
           datalab_test_assert(inspection.chunk_count == 2u, "generic pack should expose both chunks") &&
           datalab_test_assert(strcmp(inspection.family, "Generic core pack") == 0,
                               "unknown pack should retain generic classification") &&
           datalab_test_assert(strcmp(inspection.chunks[0].type, "TEST") == 0,
                               "generic pack should preserve first chunk type") &&
           datalab_test_assert(strstr(summary, "2 CHUNKS") != NULL,
                               "generic pack summary should be informative") &&
           datalab_test_assert(strstr(chunk, "DATA") != NULL,
                               "generic pack chunk summary should be informative");
}

static int test_wrapper_lifecycle_stub_dispatch_and_shutdown(void) {
    DatalabAppRuntime runtime;
    DatalabAppContext ctx;
    int rc = 0;

    datalab_app_runtime_init(&runtime);
    runtime.no_gui = 1;
    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = &runtime;
    ctx.stage = DATALAB_APP_STAGE_STATE_SEEDED;
    ctx.runtime_dispatch = datalab_test_wrapper_lifecycle_dispatch;
    ctx.wrapper_error = DATALAB_WRAPPER_ERROR_NONE;
    ctx.ownership.bootstrap_owned = 1;
    ctx.ownership.config_owned = 1;
    ctx.ownership.state_seed_owned = 1;

    g_wrapper_lifecycle_dispatch_calls = 0;
    g_wrapper_lifecycle_runtime = NULL;
    rc = datalab_app_run_loop_ctx(&ctx);
    if (!datalab_test_assert(rc == 0, "stub dispatch wrapper run loop should succeed") ||
        !datalab_test_assert(g_wrapper_lifecycle_dispatch_calls == 1,
                             "stub dispatch should be called exactly once") ||
        !datalab_test_assert(g_wrapper_lifecycle_runtime == &runtime,
                             "stub dispatch should receive runtime ownership") ||
        !datalab_test_assert(ctx.stage == DATALAB_APP_STAGE_LOOP_COMPLETED,
                             "stub dispatch should advance wrapper to loop completed") ||
        !datalab_test_assert(ctx.dispatch_summary.dispatch_count == 1u,
                             "wrapper dispatch summary should count handoff") ||
        !datalab_test_assert(ctx.dispatch_summary.dispatch_succeeded == 1,
                             "wrapper dispatch summary should mark success") ||
        !datalab_test_assert(ctx.dispatch_summary.last_dispatch_exit_code == 0,
                             "wrapper dispatch summary should record exit code") ||
        !datalab_test_assert(ctx.wrapper_error == DATALAB_WRAPPER_ERROR_NONE,
                             "successful stub dispatch should not set wrapper error") ||
        !datalab_test_assert(ctx.ownership.dispatch_owned == 1 &&
                             ctx.ownership.run_loop_handoff_owned == 1 &&
                             ctx.ownership.runtime_owned == 1,
                             "wrapper should own dispatch, handoff, and runtime before shutdown")) {
        return 0;
    }

    datalab_app_shutdown_ctx(&ctx);
    return datalab_test_assert(ctx.stage == DATALAB_APP_STAGE_SHUTDOWN_COMPLETED,
                               "shutdown should mark wrapper stage complete") &&
           datalab_test_assert(ctx.ownership.shutdown_owned == 1,
                               "shutdown should mark shutdown ownership") &&
           datalab_test_assert(ctx.ownership.bootstrap_owned == 0 &&
                               ctx.ownership.config_owned == 0 &&
                               ctx.ownership.state_seed_owned == 0 &&
                               ctx.ownership.dispatch_owned == 0 &&
                               ctx.ownership.run_loop_handoff_owned == 0 &&
                               ctx.ownership.runtime_owned == 0,
                               "shutdown should release earlier lifecycle ownership");
}

static int test_unsupported_extension_sets_bounded_error(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char invalid_path[PATH_MAX];
    char *argv[] = { (char *)"datalab", (char *)"--pack", invalid_path, (char *)"--no-gui" };
    snprintf(invalid_path, sizeof(invalid_path), "%s/invalid_input.txt", temp_dir);
    if (!datalab_test_write_text_file(invalid_path, "not a pack\n")) {
        fprintf(stderr, "contract: failed to create invalid input fixture\n");
        return 0;
    }
    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(datalab_app_bootstrap(4, argv, &runtime) == 0, "bootstrap failed for invalid extension")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for invalid extension")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) == 2, "unsupported extension should return bounded load error")) {
        return 0;
    }
    if (!datalab_test_assert(strstr(runtime.last_load_error, "unsupported input file extension") != NULL,
                             "unsupported extension should preserve a clear loader error")) {
        return 0;
    }
    if (!datalab_test_assert(strstr(runtime.last_load_error, "input=invalid_input.txt") != NULL,
                             "unsupported extension should include bounded input context")) {
        return 0;
    }
    if (!datalab_test_assert(strlen(runtime.last_load_error) < sizeof(runtime.last_load_error),
                             "unsupported extension diagnostic should stay bounded")) {
        return 0;
    }
    return 1;
}

static int test_cli_input_root_precedence(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char previous_cwd[PATH_MAX];
    char persisted_root[PATH_MAX];
    char cli_root[PATH_MAX];
    char persisted_path[PATH_MAX];
    char *argv[] = { (char *)"datalab", (char *)"--input-root", cli_root };
    int ok = 0;

    snprintf(persisted_root, sizeof(persisted_root), "%s/persisted_root", temp_dir);
    snprintf(cli_root, sizeof(cli_root), "%s/cli_root", temp_dir);
    snprintf(persisted_path, sizeof(persisted_path), "%s/data/runtime/input_root.txt", temp_dir);

    if (!datalab_test_enter_temp_runtime_root(temp_dir, previous_cwd, sizeof(previous_cwd))) {
        fprintf(stderr, "contract: failed to enter temp runtime root\n");
        return 0;
    }
    if (!datalab_test_write_text_file(persisted_path, persisted_root)) {
        datalab_test_restore_cwd(previous_cwd);
        fprintf(stderr, "contract: failed to write persisted input root\n");
        return 0;
    }

    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(3, argv, &runtime) == 0, "bootstrap failed for cli input-root") &&
        datalab_test_assert(runtime.input_root_from_cli == 1, "cli input-root flag should be tracked") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for cli input-root") &&
        datalab_test_assert(strcmp(runtime.input_root, cli_root) == 0, "cli input-root should override persisted root")) {
        ok = 1;
    }

    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_recent_input_root_mru_is_unique_and_persists(const char *temp_dir) {
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char recent[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP] = {{0}};
    char loaded[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP] = {{0}};
    char input_root[DATALAB_APP_PATH_CAP] = "";
    char path[DATALAB_APP_PATH_CAP];
    size_t recent_count = 0u;
    size_t loaded_count = 0u;
    size_t i = 0u;
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/recent_root_mru", temp_dir);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd))) {
        fprintf(stderr, "contract: failed to enter recent-root prefs root\n");
        return 0;
    }

    datalab_recent_input_roots_add(recent, &recent_count, DATALAB_RECENT_INPUT_ROOT_LIMIT, "/tmp/alpha/");
    datalab_recent_input_roots_add(recent, &recent_count, DATALAB_RECENT_INPUT_ROOT_LIMIT, "/tmp/beta");
    datalab_recent_input_roots_add(recent, &recent_count, DATALAB_RECENT_INPUT_ROOT_LIMIT, "/tmp/gamma");
    datalab_recent_input_roots_add(recent, &recent_count, DATALAB_RECENT_INPUT_ROOT_LIMIT, "/tmp/beta/");
    if (!datalab_test_assert(recent_count == 3u, "reopening a directory should not grow MRU history") ||
        !datalab_test_assert(strcmp(recent[0], "/tmp/beta") == 0, "reopened directory should move to first") ||
        !datalab_test_assert(strcmp(recent[1], "/tmp/gamma") == 0, "newer directory should shift after promotion") ||
        !datalab_test_assert(strcmp(recent[2], "/tmp/alpha") == 0, "older directory should remain in history")) {
        goto cleanup;
    }

    for (i = 0u; i <= DATALAB_RECENT_INPUT_ROOT_LIMIT; ++i) {
        snprintf(path, sizeof(path), "/tmp/datalab_recent_%zu", i);
        datalab_recent_input_roots_add(recent, &recent_count, DATALAB_RECENT_INPUT_ROOT_LIMIT, path);
    }
    if (!datalab_test_assert(recent_count == DATALAB_RECENT_INPUT_ROOT_LIMIT, "directory MRU should retain its configured capacity") ||
        !datalab_test_assert(strcmp(recent[0], "/tmp/datalab_recent_48") == 0, "latest directory should be first") ||
        !datalab_test_assert(strcmp(recent[DATALAB_RECENT_INPUT_ROOT_LIMIT - 1u], "/tmp/datalab_recent_1") == 0,
                             "oldest directory should be evicted when history is full")) {
        goto cleanup;
    }

    if (!datalab_input_root_select_recent(input_root, sizeof(input_root), recent, &recent_count,
                                          DATALAB_RECENT_INPUT_ROOT_LIMIT, "/tmp/datalab_recent_17") ||
        !datalab_test_assert(strcmp(input_root, "/tmp/datalab_recent_17") == 0, "selected directory should become active root") ||
        !datalab_test_assert(recent_count == DATALAB_RECENT_INPUT_ROOT_LIMIT, "selecting an existing directory should not duplicate history") ||
        !datalab_test_assert(strcmp(recent[0], "/tmp/datalab_recent_17") == 0, "selected directory should move to first") ||
        !datalab_test_assert(strcmp(recent[1], "/tmp/datalab_recent_48") == 0, "previous first directory should shift down")) {
        goto cleanup;
    }
    for (i = 0u; i < recent_count; ++i) {
        size_t prior = 0u;
        for (prior = 0u; prior < i; ++prior) {
            if (!datalab_test_assert(strcmp(recent[prior], recent[i]) != 0, "directory MRU should contain unique entries")) {
                goto cleanup;
            }
        }
    }
    if (!datalab_runtime_prefs_save_recent_input_roots(recent, recent_count) ||
        !datalab_runtime_prefs_load_recent_input_roots(loaded, DATALAB_RECENT_INPUT_ROOT_LIMIT, &loaded_count) ||
        !datalab_test_assert(loaded_count == recent_count, "persisted directory history count should reload") ||
        !datalab_test_assert(strcmp(loaded[0], recent[0]) == 0, "persisted directory history should preserve most recent entry") ||
        !datalab_test_assert(strcmp(loaded[DATALAB_RECENT_INPUT_ROOT_LIMIT - 1u], recent[DATALAB_RECENT_INPUT_ROOT_LIMIT - 1u]) == 0,
                             "persisted directory history should preserve oldest retained entry")) {
        goto cleanup;
    }
    ok = 1;

cleanup:
    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_theme_preset_persists_across_config_reload(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char *argv[] = { (char *)"datalab" };
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/theme_prefs", temp_dir);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd))) {
        fprintf(stderr, "contract: failed to enter theme prefs root\n");
        return 0;
    }
    if (!datalab_runtime_prefs_save_theme_preset_id(
            (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT)) {
        datalab_test_restore_cwd(previous_cwd);
        fprintf(stderr, "contract: failed to save theme preset\n");
        return 0;
    }
    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0,
                            "bootstrap failed for persisted theme") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                            "config load failed for persisted theme") &&
        datalab_test_assert(runtime.workspace_authoring_theme_preset_id ==
                            (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_SOFT_LIGHT,
                            "saved theme should load into the next runtime")) {
        ok = 1;
    }
    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_missing_prefs_stay_quiet(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char previous_cwd[PATH_MAX];
    char quiet_root[PATH_MAX];
    char *argv[] = { (char *)"datalab" };
    int ok = 0;

    snprintf(quiet_root, sizeof(quiet_root), "%s/prefs_quiet", temp_dir);
    if (!datalab_test_enter_temp_runtime_root(quiet_root, previous_cwd, sizeof(previous_cwd))) {
        fprintf(stderr, "contract: failed to enter quiet prefs root\n");
        return 0;
    }

    datalab_runtime_prefs_clear_diagnostic();
    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0, "bootstrap failed for quiet prefs") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for quiet prefs") &&
        datalab_test_assert(datalab_runtime_prefs_last_diagnostic()[0] == '\0',
                            "missing first-run prefs should not emit diagnostics")) {
        ok = 1;
    }

    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_malformed_prefs_report_diagnostic(const char *temp_dir) {
    int loaded_step = 0;
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char text_zoom_path[PATH_MAX];
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/prefs_malformed", temp_dir);
    snprintf(text_zoom_path, sizeof(text_zoom_path), "%s/data/runtime/text_zoom_step.txt", prefs_root);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd))) {
        fprintf(stderr, "contract: failed to enter malformed prefs root\n");
        return 0;
    }
    if (!datalab_test_write_text_file(text_zoom_path, "not-a-number\n")) {
        datalab_test_restore_cwd(previous_cwd);
        fprintf(stderr, "contract: failed to write malformed prefs fixture\n");
        return 0;
    }

    datalab_runtime_prefs_clear_diagnostic();
    if (datalab_test_assert(datalab_runtime_prefs_load_text_zoom_step(&loaded_step) == 0,
                            "malformed text zoom prefs should fail to load") &&
        datalab_test_assert(strstr(datalab_runtime_prefs_last_diagnostic(), "prefs load failed") != NULL,
                            "malformed prefs should report load diagnostic") &&
        datalab_test_assert(strstr(datalab_runtime_prefs_last_diagnostic(), "text_zoom_step.txt") != NULL,
                            "malformed prefs diagnostic should include bounded path context")) {
        ok = 1;
    }

    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_prefs_save_failure_reports_diagnostic(const char *temp_dir) {
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char data_path[PATH_MAX];
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/prefs_save_failure", temp_dir);
    snprintf(data_path, sizeof(data_path), "%s/data", prefs_root);
    if (!datalab_test_mkdir_if_needed(prefs_root)) {
        fprintf(stderr, "contract: failed to create prefs save root\n");
        return 0;
    }
    if (!getcwd(previous_cwd, sizeof(previous_cwd))) {
        return 0;
    }
    if (!datalab_test_write_text_file(data_path, "not a directory\n")) {
        return 0;
    }
    if (chdir(prefs_root) != 0) {
        return 0;
    }

    datalab_runtime_prefs_clear_diagnostic();
    if (datalab_test_assert(datalab_runtime_prefs_save_text_zoom_step(2) == 0,
                            "prefs save should fail when runtime directory cannot be created") &&
        datalab_test_assert(strstr(datalab_runtime_prefs_last_diagnostic(), "prefs save failed") != NULL,
                            "prefs save failure should report diagnostic") &&
        datalab_test_assert(strstr(datalab_runtime_prefs_last_diagnostic(), "text_zoom_step.txt") != NULL,
                            "prefs save diagnostic should include bounded path context")) {
        ok = 1;
    }

    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_render_open_failure_reports_context(void) {
    CoreResult result;
    const DatalabRenderFailureDiagnostic *diag = NULL;
    const char *summary = NULL;

    datalab_render_clear_failure_diagnostic();
    result = datalab_render_session_open(NULL);
    diag = datalab_render_last_failure_diagnostic();
    summary = datalab_render_last_failure_summary();

    return datalab_test_assert(result.code == CORE_ERR_INVALID_ARG,
                               "null render session open should fail") &&
           datalab_test_assert(diag && strcmp(diag->stage, "session_open") == 0,
                               "render open diagnostic should include stage") &&
           datalab_test_assert(strcmp(diag->route, "validate") == 0,
                               "render open diagnostic should include route") &&
           datalab_test_assert(strcmp(diag->profile, "unknown") == 0,
                               "render open diagnostic should include unknown profile") &&
           datalab_test_assert(diag->result_code == CORE_ERR_INVALID_ARG,
                               "render open diagnostic should include result code") &&
           datalab_test_assert(strstr(diag->detail, "invalid render session request") != NULL,
                               "render open diagnostic should include bounded detail") &&
           datalab_test_assert(summary && strstr(summary, "stage=session_open") != NULL,
                               "render open summary should include stage") &&
           datalab_test_assert(strstr(summary, "route=validate") != NULL,
                               "render open summary should include route") &&
           datalab_test_assert(strlen(summary) < 256u,
                               "render open summary should stay bounded");
}

static int test_render_submit_failure_reports_context(void) {
    DatalabFrame frame;
    DatalabAppState state;
    CoreResult result;
    const DatalabRenderFailureDiagnostic *diag = NULL;
    const char *summary = NULL;
    float density[4] = { 0.0f, 0.1f, 0.2f, 0.3f };
    float velx[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float vely[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    datalab_frame_init(&frame);
    frame.profile = DATALAB_PROFILE_PHYSICS;
    frame.width = 2u;
    frame.height = 2u;
    frame.density = density;
    frame.velx = velx;
    frame.vely = vely;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_PHYSICS);

    datalab_render_clear_failure_diagnostic();
    result = datalab_render_run_with_session(NULL, &frame, &state);
    diag = datalab_render_last_failure_diagnostic();
    summary = datalab_render_last_failure_summary();

    return datalab_test_assert(result.code == CORE_ERR_INVALID_ARG,
                               "null render submit session should fail") &&
           datalab_test_assert(diag && strcmp(diag->stage, "render_submit") == 0,
                               "render submit diagnostic should include stage") &&
           datalab_test_assert(strcmp(diag->route, "session_state") == 0,
                               "render submit diagnostic should include route") &&
           datalab_test_assert(strcmp(diag->profile, "physics") == 0,
                               "render submit diagnostic should include profile") &&
           datalab_test_assert(diag->result_code == CORE_ERR_INVALID_ARG,
                               "render submit diagnostic should include result code") &&
           datalab_test_assert(strstr(diag->detail, "render session is not open") != NULL,
                               "render submit diagnostic should include bounded detail") &&
           datalab_test_assert(summary && strstr(summary, "stage=render_submit") != NULL,
                               "render submit summary should include stage") &&
           datalab_test_assert(strstr(summary, "route=session_state") != NULL,
                               "render submit summary should include route") &&
           datalab_test_assert(strstr(summary, "profile=physics") != NULL,
                               "render submit summary should include profile") &&
           datalab_test_assert(strlen(summary) < 256u,
                               "render submit summary should stay bounded");
}

static int test_bmp_bounds_reject_oversized_inputs(void) {
    CoreResult result;
    size_t row_bytes = 0u;
    size_t image_bytes = 0u;

    result = datalab_input_image_bounds(32u, 16u, &row_bytes, &image_bytes);
    if (!datalab_test_assert(result.code == CORE_OK, "ordinary bmp bounds should pass") ||
        !datalab_test_assert(row_bytes == 128u, "ordinary bmp row bytes should be exact") ||
        !datalab_test_assert(image_bytes == 2048u, "ordinary bmp image bytes should be exact")) {
        return 0;
    }

    result = datalab_input_image_bounds(0u, 16u, &row_bytes, &image_bytes);
    if (!datalab_test_assert(result.code == CORE_ERR_FORMAT, "zero-width bmp should fail bounds")) {
        return 0;
    }

    result = datalab_input_image_bounds(DATALAB_INPUT_IMAGE_MAX_DIMENSION + 1u,
                                        16u,
                                        &row_bytes,
                                        &image_bytes);
    if (!datalab_test_assert(result.code == CORE_ERR_FORMAT, "oversized bmp width should fail bounds")) {
        return 0;
    }

    result = datalab_input_image_bounds(DATALAB_INPUT_IMAGE_MAX_DIMENSION,
                                        DATALAB_INPUT_IMAGE_MAX_DIMENSION,
                                        &row_bytes,
                                        &image_bytes);
    if (!datalab_test_assert(result.code == CORE_ERR_FORMAT, "oversized bmp pixel count should fail bounds")) {
        return 0;
    }

    return 1;
}

int main(void) {
    char temp_dir[PATH_MAX];
    const char *default_pack = DATALAB_TEST_DEFAULT_PACK;

    if (!datalab_test_make_temp_dir(temp_dir, sizeof(temp_dir))) {
        fprintf(stderr, "contract: failed to create temp dir\n");
        return 1;
    }

    if (!test_headless_requires_pack()) {
        return 1;
    }
    if (!test_valid_headless_pack_state_seed(default_pack)) {
        return 1;
    }
    if (!test_selected_pack_path_fallback(default_pack)) {
        return 1;
    }
    if (!test_generated_bmp_load_path_and_state_seed(temp_dir)) {
        return 1;
    }
    if (!test_generated_png_load_path(temp_dir)) {
        return 1;
    }
    if (!test_generic_pack_inspection(temp_dir)) {
        return 1;
    }
    if (!test_wrapper_lifecycle_stub_dispatch_and_shutdown()) {
        return 1;
    }
    if (!test_unsupported_extension_sets_bounded_error(temp_dir)) {
        return 1;
    }
    if (!test_cli_input_root_precedence(temp_dir)) {
        return 1;
    }
    if (!test_recent_input_root_mru_is_unique_and_persists(temp_dir)) {
        return 1;
    }
    if (!test_theme_preset_persists_across_config_reload(temp_dir)) {
        return 1;
    }
    if (!test_missing_prefs_stay_quiet(temp_dir)) {
        return 1;
    }
    if (!test_malformed_prefs_report_diagnostic(temp_dir)) {
        return 1;
    }
    if (!test_prefs_save_failure_reports_diagnostic(temp_dir)) {
        return 1;
    }
    if (!test_render_open_failure_reports_context()) {
        return 1;
    }
    if (!test_render_submit_failure_reports_context()) {
        return 1;
    }
    if (!test_bmp_bounds_reject_oversized_inputs()) {
        return 1;
    }

    puts("datalab app contract test passed");
    return 0;
}
