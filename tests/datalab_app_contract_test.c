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
#include "app/datalab_viewer_session_prefs.h"
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

static int datalab_test_write_tiny_png(const char *path, int set_srgb, int set_gamma) {
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
    if (set_srgb) png_set_sRGB(png, info, PNG_sRGB_INTENT_PERCEPTUAL);
    if (set_gamma) png_set_gAMA(png, info, 0.45455);
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
        !datalab_test_assert(frame.drawing_rgba != NULL, "generated bmp should produce rgba pixels") ||
        !datalab_test_assert(frame.image_metadata.format == DATALAB_IMAGE_FORMAT_BMP &&
                             frame.image_metadata.transfer == DATALAB_IMAGE_TRANSFER_UNTAGGED_SRGB_ASSUMED,
                             "bmp should expose its untagged sRGB assumption")) {
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
    if (!datalab_test_write_tiny_png(png_path, 0, 0)) {
        fprintf(stderr, "contract: failed to create png fixture\n");
        return 0;
    }
    result = datalab_load_input_file(png_path, &frame);
    if (!datalab_test_assert(result.code == CORE_OK, "generated png should load through input loader") ||
        !datalab_test_assert(datalab_input_file_is_png(png_path), "png extension should be supported") ||
        !datalab_test_assert(frame.profile == DATALAB_PROFILE_IMAGE, "generated png should seed image profile") ||
        !datalab_test_assert(frame.width == 2u && frame.height == 2u, "generated png dimensions should load") ||
        !datalab_test_assert(frame.drawing_rgba != NULL, "generated png should produce rgba pixels") ||
        !datalab_test_assert(frame.image_metadata.format == DATALAB_IMAGE_FORMAT_PNG &&
                             frame.image_metadata.transfer == DATALAB_IMAGE_TRANSFER_UNTAGGED_SRGB_ASSUMED,
                             "untagged png should expose its declared assumption")) {
        datalab_frame_free(&frame);
        return 0;
    }
    datalab_frame_free(&frame);
    return 1;
}

static int test_png_tag_metadata(const char *temp_dir) {
    DatalabFrame srgb_frame = {0};
    DatalabFrame gamma_frame = {0};
    CoreResult srgb_result;
    CoreResult gamma_result;
    char srgb_path[PATH_MAX];
    char gamma_path[PATH_MAX];
    snprintf(srgb_path, sizeof(srgb_path), "%s/tagged_srgb.png", temp_dir);
    snprintf(gamma_path, sizeof(gamma_path), "%s/tagged_gamma.png", temp_dir);
    if (!datalab_test_write_tiny_png(srgb_path, 1, 0) || !datalab_test_write_tiny_png(gamma_path, 0, 1)) return 0;
    srgb_result = datalab_load_input_file(srgb_path, &srgb_frame);
    gamma_result = datalab_load_input_file(gamma_path, &gamma_frame);
    if (!datalab_test_assert(srgb_result.code == CORE_OK && srgb_frame.image_metadata.png_srgb_present &&
                             srgb_frame.image_metadata.transfer == DATALAB_IMAGE_TRANSFER_SRGB,
                             "PNG sRGB tags must be surfaced as raw 8-bit sRGB metadata") ||
        !datalab_test_assert(gamma_result.code == CORE_OK && gamma_frame.image_metadata.png_gamma_present &&
                             gamma_frame.image_metadata.transfer == DATALAB_IMAGE_TRANSFER_GAMA &&
                             gamma_frame.image_metadata.png_gamma > 0.45 && gamma_frame.image_metadata.png_gamma < 0.46,
                             "PNG gAMA tags must be surfaced without claiming color transformation")) {
        datalab_frame_free(&srgb_frame);
        datalab_frame_free(&gamma_frame);
        return 0;
    }
    datalab_frame_free(&srgb_frame);
    datalab_frame_free(&gamma_frame);
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

static int test_last_opened_artifact_restores_or_fails_safe(const char *temp_dir) {
    DatalabAppRuntime runtime;
    DatalabViewerSession viewer_session;
    DatalabRasterViewportState viewer_viewport;
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char artifact_root[PATH_MAX];
    char artifact_path[PATH_MAX];
    char missing_path[PATH_MAX];
    char *argv[] = { (char *)"datalab" };
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/last_opened_artifact", temp_dir);
    snprintf(artifact_root, sizeof(artifact_root), "%s/frames", prefs_root);
    snprintf(artifact_path, sizeof(artifact_path), "%s/frame_0000.bmp", artifact_root);
    snprintf(missing_path, sizeof(missing_path), "%s/renamed_frame.bmp", artifact_root);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd)) ||
        !datalab_test_mkdir_if_needed(artifact_root) ||
        !datalab_test_write_tiny_bmp(artifact_path) ||
        !datalab_runtime_prefs_save_input_root(artifact_root) ||
        !datalab_runtime_prefs_save_last_opened_input_file(artifact_path) ||
        !datalab_runtime_prefs_save_startup_surface(DATALAB_STARTUP_SURFACE_VIEWER)) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }
    datalab_raster_viewport_state_init(&viewer_viewport);
    datalab_raster_viewport_sync_state(&viewer_viewport, 800, 600, 2u, 2u);
    viewer_viewport.fit_mode = 0;
    viewer_viewport.reset_requested = 0;
    viewer_viewport.viewport.zoom = 3.0f;
    viewer_viewport.viewport.pan_x = 21.0f;
    viewer_viewport.viewport.pan_y = -11.0f;
    datalab_viewer_session_capture(&viewer_session,
                                   artifact_path,
                                   &viewer_viewport,
                                   1,
                                   DATALAB_PLAYBACK_MODE_BOUNCE,
                                   4,
                                   1,
                                   DATALAB_SAMPLING_MODE_LINEAR);
    if (!datalab_viewer_session_save(&viewer_session)) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }

    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0,
                             "bootstrap failed for last-opened artifact") ||
        !datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                             "config load failed for last-opened artifact") ||
        !datalab_test_assert(runtime.reopened_last_input_file && runtime.pack_path == runtime.selected_pack_path,
                             "valid last-opened artifact should be selected for startup") ||
        !datalab_test_assert(strcmp(runtime.input_root, artifact_root) == 0,
                             "saved input root should be retained independently from the file") ||
        !datalab_test_assert(runtime.input_catalog.file_count == 1u &&
                                 datalab_input_catalog_file_is_current(&runtime.input_catalog,
                                                                       artifact_root,
                                                                       "frame_0000.bmp"),
                             "viewer restoration must attach a current catalog before loading") ||
        !datalab_test_assert(datalab_app_state_seed(&runtime) == 0 && runtime.frame_loaded,
                             "valid last-opened artifact should reopen into runtime") ||
        !datalab_test_assert(runtime.playback_active && runtime.playback_mode == DATALAB_PLAYBACK_MODE_BOUNCE &&
                                 runtime.playback_speed_index == 4 && runtime.session_hud_collapsed &&
                                 runtime.sampling_mode == DATALAB_SAMPLING_MODE_LINEAR &&
                                 !runtime.raster_viewport.fit_mode && runtime.raster_viewport.viewport.zoom == 3.0f &&
                                 runtime.raster_viewport.viewport.pan_x == 21.0f && runtime.raster_viewport.viewport.pan_y == -11.0f,
                             "viewer session should restore current behavior only after the image loads")) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }

    if (!datalab_runtime_prefs_save_input_root(artifact_root) ||
        !datalab_runtime_prefs_save_last_opened_input_file(missing_path) ||
        !datalab_runtime_prefs_save_startup_surface(DATALAB_STARTUP_SURFACE_VIEWER)) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }
    datalab_viewer_session_capture(&viewer_session,
                                   missing_path,
                                   &viewer_viewport,
                                   0,
                                   DATALAB_PLAYBACK_MODE_LOOP,
                                   DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT,
                                   0,
                                   DATALAB_SAMPLING_MODE_DEFAULT);
    if (!datalab_viewer_session_save(&viewer_session)) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }
    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0,
                            "bootstrap failed for missing last-opened artifact") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                            "config load failed for missing last-opened artifact") &&
        datalab_test_assert(datalab_app_state_seed(&runtime) == 0,
                            "missing last-opened artifact should fall back to picker") &&
        datalab_test_assert(!runtime.reopened_last_input_file && !runtime.frame_loaded && runtime.pack_path == NULL,
                            "missing remembered file must not enter a stale viewer") &&
        datalab_test_assert(strcmp(runtime.input_root, artifact_root) == 0 &&
                                runtime.input_catalog.file_count == 1u,
                            "missing remembered file must retain the valid root and picker catalog")) {
        ok = 1;
    }
    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_async_decode_generation_cancellation_contract(const char *temp_dir) {
    DatalabAppRuntime runtime;
    DatalabAppState app_state;
    char root[PATH_MAX];
    char first_path[PATH_MAX];
    char selected_path[PATH_MAX];
    char corrupt_path[PATH_MAX];
    int applied = 0;
    int attempt = 0;
    int ok = 0;

    snprintf(root, sizeof(root), "%s/async_decode", temp_dir);
    snprintf(first_path, sizeof(first_path), "%s/frame_0000.bmp", root);
    snprintf(selected_path, sizeof(selected_path), "%s/frame_0001.bmp", root);
    snprintf(corrupt_path, sizeof(corrupt_path), "%s/frame_0002.bmp", root);
    if (!datalab_test_mkdir_if_needed(root) || !datalab_test_write_tiny_bmp(first_path) ||
        !datalab_test_write_tiny_bmp(selected_path) || !datalab_test_write_text_file(corrupt_path, "not a bmp") ||
        SDL_Init(SDL_INIT_EVENTS) != 0) {
        return 0;
    }
    datalab_app_runtime_init(&runtime);
    datalab_app_state_init(&app_state, NULL, DATALAB_PROFILE_IMAGE);
    app_state.runtime_owner = &runtime;
    app_state.async_decode = &runtime.async_decode;
    if (!datalab_test_assert(datalab_async_decode_start(&runtime.async_decode),
                             "async decode infrastructure should start") ||
        !datalab_test_assert(datalab_async_decode_request_selected(&runtime.async_decode, first_path) &&
                                 datalab_async_decode_request_selected(&runtime.async_decode, selected_path),
                             "selected requests should advance the generation and enqueue")) {
        datalab_async_decode_shutdown(&runtime.async_decode);
        SDL_Quit();
        return 0;
    }
    for (attempt = 0; attempt < 250 && !applied; ++attempt) {
        applied = datalab_async_decode_pump(&runtime.async_decode, &runtime, &app_state);
        if (!applied) {
            SDL_Delay(2u);
        }
    }
    ok = datalab_test_assert(applied &&
                                 datalab_async_decode_pending_selected(&runtime.async_decode) != NULL &&
                                 datalab_async_decode_commit_pending_selected(&runtime.async_decode, &runtime, &app_state) &&
                                 runtime.frame_loaded &&
                                 strcmp(runtime.selected_pack_path, selected_path) == 0 &&
                                 runtime.frame.profile == DATALAB_PROFILE_IMAGE,
                             "only the latest selected decode generation may commit after render-thread presentation");
    if (ok && datalab_test_assert(datalab_async_decode_request_selected(&runtime.async_decode, corrupt_path),
                                  "a corrupt numbered neighbor should still reach the async decoder")) {
        applied = 0;
        for (attempt = 0; attempt < 250; ++attempt) {
            applied |= datalab_async_decode_pump(&runtime.async_decode, &runtime, &app_state);
            SDL_Delay(2u);
        }
        ok = datalab_test_assert(!applied && runtime.frame_loaded &&
                                     strcmp(runtime.selected_pack_path, selected_path) == 0 &&
                                     strstr(runtime.last_load_error, "keeping current frame") != NULL &&
                                     strstr(runtime.last_load_error, "F5 retry") != NULL,
                                 "a corrupt middle frame must preserve the last valid frame with retry guidance");
    } else {
        ok = 0;
    }
    datalab_async_decode_shutdown(&runtime.async_decode);
    SDL_Quit();
    if (runtime.frame_loaded) {
        datalab_frame_free(&runtime.frame);
    }
    return ok;
}

static int test_picker_surface_does_not_autoload_last_file(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char artifact_root[PATH_MAX];
    char artifact_path[PATH_MAX];
    char *argv[] = { (char *)"datalab" };
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/picker_surface", temp_dir);
    snprintf(artifact_root, sizeof(artifact_root), "%s/frames", prefs_root);
    snprintf(artifact_path, sizeof(artifact_path), "%s/frame_0000.bmp", artifact_root);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd)) ||
        !datalab_test_mkdir_if_needed(artifact_root) ||
        !datalab_test_write_tiny_bmp(artifact_path) ||
        !datalab_runtime_prefs_save_last_opened_input_file(artifact_path) ||
        !datalab_runtime_prefs_save_input_root(artifact_root) ||
        !datalab_runtime_prefs_save_startup_surface(DATALAB_STARTUP_SURFACE_PICKER)) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }

    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0,
                            "bootstrap failed for picker surface") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                            "config load failed for picker surface") &&
        datalab_test_assert(runtime.pack_path == NULL && !runtime.reopened_last_input_file,
                            "picker surface must not auto-load the last file") &&
        datalab_test_assert(strcmp(runtime.input_root, artifact_root) == 0,
                            "picker surface should still restore its directory")) {
        ok = 1;
    }
    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_runtime_seed_ignores_uninitialized_app_state(void) {
    DatalabAppRuntime runtime;
    DatalabAppState app_state;
    datalab_app_runtime_init(&runtime);
    snprintf(runtime.input_root, sizeof(runtime.input_root), "%s", "/tmp/datalab_seed_root");
    snprintf(runtime.selected_pack_path, sizeof(runtime.selected_pack_path), "%s", "/tmp/datalab_seed_root/frame.bmp");
    runtime.pack_path = runtime.selected_pack_path;
    runtime.text_zoom_step = 2;
    memset(&app_state, 0xa5, sizeof(app_state));

    datalab_runtime_copy_to_app_state(&runtime, &app_state, 1);
    return datalab_test_assert(app_state.pack_path == runtime.selected_pack_path,
                               "runtime seed should initialize the active file pointer") &&
           datalab_test_assert(strcmp(app_state.input_root, runtime.input_root) == 0,
                               "runtime seed should initialize the input root") &&
           datalab_test_assert(app_state.text_zoom_step == runtime.text_zoom_step,
                               "runtime seed should initialize persisted presentation state") &&
           datalab_test_assert(app_state.panel_rescan_requested == 1,
                               "runtime seed should preserve the requested first panel scan");
}

/* Selecting a directory is useful before opening a particular artifact.  Keep
 * that accepted root and its direct-child scan intact across a picker restart. */
static int test_selected_input_root_reopens_with_its_artifacts(const char *temp_dir) {
    DatalabAppRuntime runtime;
    DatalabSupportedFileScanResult scan;
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char artifact_root[PATH_MAX];
    char artifact_path[PATH_MAX];
    char files[8][DATALAB_APP_PATH_CAP] = {{0}};
    char *argv[] = { (char *)"datalab" };
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/selected_root_reopen", temp_dir);
    snprintf(artifact_root, sizeof(artifact_root), "%s/frames", prefs_root);
    snprintf(artifact_path, sizeof(artifact_path), "%s/frame_0000.bmp", artifact_root);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd)) ||
        !datalab_test_mkdir_if_needed(artifact_root) ||
        !datalab_test_write_tiny_bmp(artifact_path) ||
        !datalab_runtime_prefs_save_input_root(artifact_root)) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }

    datalab_app_runtime_init(&runtime);
    scan = datalab_scan_supported_files(artifact_root, files, 8u);
    if (datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0,
                            "bootstrap failed for selected input root") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                            "config load failed for selected input root") &&
        datalab_test_assert(!runtime.reopened_last_input_file && runtime.pack_path == NULL,
                            "a selected root alone should return to the picker") &&
        datalab_test_assert(strcmp(runtime.input_root, artifact_root) == 0,
                            "selected input root should be restored exactly") &&
        datalab_test_assert(!scan.root_unavailable && scan.file_count == 1u &&
                                strcmp(files[0], "frame_0000.bmp") == 0,
                            "restored input root must rescan its BMP artifacts")) {
        ok = 1;
    }
    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

static int test_supported_file_catalog_natural_order_and_overflow(const char *temp_dir) {
    DatalabSupportedFileScanResult scan;
    char catalog_root[PATH_MAX];
    char path[PATH_MAX];
    char files[2][DATALAB_APP_PATH_CAP] = {{0}};
    char status[160] = "";

    snprintf(catalog_root, sizeof(catalog_root), "%s/input_catalog", temp_dir);
    if (!datalab_test_mkdir_if_needed(catalog_root)) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/frame_10.bmp", catalog_root);
    if (!datalab_test_write_text_file(path, "fixture")) return 0;
    snprintf(path, sizeof(path), "%s/frame_2.bmp", catalog_root);
    if (!datalab_test_write_text_file(path, "fixture")) return 0;
    snprintf(path, sizeof(path), "%s/frame_001.pack", catalog_root);
    if (!datalab_test_write_text_file(path, "fixture")) return 0;
    snprintf(path, sizeof(path), "%s/notes.txt", catalog_root);
    if (!datalab_test_write_text_file(path, "ignored")) return 0;

    scan = datalab_scan_supported_files(catalog_root, files, 2u);
    datalab_format_supported_file_scan_status(&scan,
                                              catalog_root,
                                              "choose folder",
                                              status,
                                              sizeof(status));
    return datalab_test_assert(!scan.root_unavailable && !scan.allocation_failed,
                               "catalog scan should succeed") &&
           datalab_test_assert(scan.file_count == 2u && scan.available_file_count == 3u && scan.truncated,
                               "bounded catalog consumer should report its full available count") &&
           datalab_test_assert(strcmp(files[0], "frame_001.pack") == 0 &&
                                   strcmp(files[1], "frame_2.bmp") == 0,
                               "catalog should use natural numeric frame ordering") &&
           datalab_test_assert(scan.sequence_gap_count == 7u,
                               "matching numbered BMP frames must report each missing source-frame index") &&
           datalab_test_assert(strstr(status, "showing 2 of 3 supported files") != NULL,
                               "catalog overflow status should be truthful and actionable");
}

static int test_retained_input_catalog_refresh_contract(const char *temp_dir) {
    DatalabInputCatalog catalog;
    CoreResult result;
    char root[PATH_MAX];
    char path[PATH_MAX];
    uint64_t initial_refresh_count = 0u;

    snprintf(root, sizeof(root), "%s/retained_input_catalog", temp_dir);
    snprintf(path, sizeof(path), "%s/frame_1.bmp", root);
    if (!datalab_test_mkdir_if_needed(root) || !datalab_test_write_text_file(path, "fixture")) {
        return 0;
    }
    datalab_input_catalog_init(&catalog);
    result = datalab_input_catalog_refresh(&catalog,
                                           root,
                                           DATALAB_INPUT_CATALOG_REFRESH_INITIAL);
    if (!datalab_test_assert(result.code == CORE_OK && catalog.file_count == 1u &&
                                 catalog.last_refresh_reason == DATALAB_INPUT_CATALOG_REFRESH_INITIAL &&
                                 catalog.refresh_count == 1u &&
                                 !datalab_input_catalog_fingerprint_changed(&catalog, root) &&
                                 datalab_input_catalog_file_is_current(&catalog, root, "frame_1.bmp"),
                             "initial retained catalog refresh should provide current catalog truth")) {
        datalab_input_catalog_destroy(&catalog);
        return 0;
    }
    initial_refresh_count = catalog.refresh_count;
    snprintf(path, sizeof(path), "%s/frame_2.bmp", root);
    if (!datalab_test_write_text_file(path, "fixture") ||
        !datalab_test_assert(datalab_input_catalog_fingerprint_changed(&catalog, root),
                             "directory mutation should invalidate the retained catalog fingerprint")) {
        datalab_input_catalog_destroy(&catalog);
        return 0;
    }
    result = datalab_input_catalog_refresh(&catalog,
                                           root,
                                           DATALAB_INPUT_CATALOG_REFRESH_FINGERPRINT_CHANGED);
    if (!datalab_test_assert(result.code == CORE_OK && catalog.file_count == 2u &&
                                 catalog.refresh_count == initial_refresh_count + 1u &&
                                 catalog.last_refresh_reason == DATALAB_INPUT_CATALOG_REFRESH_FINGERPRINT_CHANGED &&
                                 strcmp(datalab_input_catalog_refresh_reason_name(catalog.last_refresh_reason),
                                        "fingerprint-change") == 0,
                             "fingerprint refresh should replace the retained catalog and retain diagnostics")) {
        datalab_input_catalog_destroy(&catalog);
        return 0;
    }
    if (unlink(path) != 0 ||
        !datalab_test_assert(!datalab_input_catalog_file_is_current(&catalog, root, "frame_2.bmp"),
                             "a deleted entry must not be accepted from a stale retained catalog")) {
        datalab_input_catalog_destroy(&catalog);
        return 0;
    }
    result = datalab_input_catalog_refresh(&catalog,
                                           "/definitely/not/a/datalab/input/root",
                                           DATALAB_INPUT_CATALOG_REFRESH_EXPLICIT);
    if (!datalab_test_assert(result.code == CORE_ERR_IO && catalog.file_count == 0u &&
                                 !catalog.fingerprint_valid,
                             "unavailable input roots must clear retained catalog state fail-closed")) {
        datalab_input_catalog_destroy(&catalog);
        return 0;
    }
    datalab_input_catalog_destroy(&catalog);
    return 1;
}

static int test_runtime_subsystems_seed_retained_catalog_before_first_render(const char *temp_dir) {
    DatalabAppRuntime runtime;
    DatalabAppState app_state;
    CoreResult result;
    char root[PATH_MAX];
    char first_path[PATH_MAX];
    char active_path[PATH_MAX];
    int ok = 0;

    snprintf(root, sizeof(root), "%s/retained_catalog_first_render", temp_dir);
    snprintf(first_path, sizeof(first_path), "%s/frame_0000.bmp", root);
    snprintf(active_path, sizeof(active_path), "%s/frame_0001.bmp", root);
    if (!datalab_test_mkdir_if_needed(root) ||
        !datalab_test_write_tiny_bmp(first_path) || !datalab_test_write_tiny_bmp(active_path)) {
        return 0;
    }

    datalab_app_runtime_init(&runtime);
    result = datalab_input_catalog_refresh(&runtime.input_catalog,
                                           root,
                                           DATALAB_INPUT_CATALOG_REFRESH_INITIAL);
    snprintf(runtime.input_root, sizeof(runtime.input_root), "%s", root);
    snprintf(runtime.selected_pack_path, sizeof(runtime.selected_pack_path), "%s", active_path);
    runtime.pack_path = runtime.selected_pack_path;
    runtime.frame.profile = DATALAB_PROFILE_IMAGE;
    if (result.code != CORE_OK || datalab_app_subsystems_init(&runtime, &app_state) != 0) {
        datalab_app_shutdown(&runtime);
        return 0;
    }

    ok = datalab_test_assert(runtime.input_catalog.refresh_count == 1u,
                             "runtime subsystem seed must not rescan a current retained catalog") &&
         datalab_test_assert(datalab_session_controls_file_count() == 2u,
                             "runtime subsystem seed must populate the session cache before first render") &&
         datalab_test_assert(strcmp(datalab_session_controls_selected_file_name(&app_state), "frame_0001.bmp") == 0,
                             "runtime subsystem seed must retain the restored viewer selection") &&
         datalab_test_assert(strstr(datalab_session_controls_catalog_status(), "found 2 supported files") != NULL,
                             "runtime subsystem seed must publish a non-empty panel status before first render");
    datalab_app_shutdown(&runtime);
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

static int test_workspace_projection_persists_across_config_reload(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char *argv[] = { (char *)"datalab" };
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/workspace_projection_prefs", temp_dir);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd))) {
        fprintf(stderr, "contract: failed to enter workspace projection prefs root\n");
        return 0;
    }
    if (!datalab_runtime_prefs_save_workspace_authoring_profile_surface_ratio(0.64f)) {
        datalab_test_restore_cwd(previous_cwd);
        fprintf(stderr, "contract: failed to save workspace projection\n");
        return 0;
    }
    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0,
                            "bootstrap failed for persisted workspace projection") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                            "config load failed for persisted workspace projection") &&
        datalab_test_assert(runtime.workspace_authoring_profile_surface_ratio > 0.639f &&
                            runtime.workspace_authoring_profile_surface_ratio < 0.641f,
                            "saved projection should load into the next runtime") &&
        datalab_test_assert(!datalab_runtime_prefs_save_workspace_authoring_profile_surface_ratio(0.81f),
                            "out-of-range projection should be rejected")) {
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

static int test_authoring_prefs_are_atomic_and_fail_closed(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char previous_cwd[PATH_MAX];
    char prefs_root[PATH_MAX];
    char text_path[PATH_MAX];
    char text_temp_path[PATH_MAX];
    char text_temp_marker_path[PATH_MAX];
    char ratio_path[PATH_MAX];
    char custom_theme_path[PATH_MAX];
    DatalabWorkspaceCustomTheme loaded_theme;
    char *argv[] = { (char *)"datalab" };
    int loaded_step = 99;
    int ok = 0;

    snprintf(prefs_root, sizeof(prefs_root), "%s/authoring_prefs_atomic", temp_dir);
    snprintf(text_path, sizeof(text_path), "%s/data/runtime/text_zoom_step.txt", prefs_root);
    snprintf(text_temp_path, sizeof(text_temp_path), "%s.tmp", text_path);
    snprintf(text_temp_marker_path, sizeof(text_temp_marker_path), "%s/hold", text_temp_path);
    snprintf(ratio_path, sizeof(ratio_path), "%s/data/runtime/workspace_authoring_projection_v1.txt", prefs_root);
    snprintf(custom_theme_path, sizeof(custom_theme_path), "%s/data/runtime/workspace_authoring_custom_theme_v1.txt", prefs_root);
    if (!datalab_test_enter_temp_runtime_root(prefs_root, previous_cwd, sizeof(previous_cwd))) {
        return 0;
    }
    if (!datalab_runtime_prefs_save_text_zoom_step(2) ||
        !datalab_runtime_prefs_save_workspace_authoring_profile_surface_ratio(0.64f) ||
        !datalab_test_mkdir_if_needed(text_temp_path) ||
        !datalab_test_write_text_file(text_temp_marker_path, "hold\n")) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }
    datalab_runtime_prefs_clear_diagnostic();
    if (!datalab_test_assert(!datalab_runtime_prefs_save_text_zoom_step(3),
                             "blocked atomic replacement should fail") ||
        !datalab_test_assert(datalab_runtime_prefs_load_text_zoom_step(&loaded_step) && loaded_step == 2,
                             "failed atomic replacement must retain the accepted text zoom")) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }
    if (!datalab_test_write_text_file(ratio_path, "0.64 trailing\n")) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }
    memset(&loaded_theme, 0x5a, sizeof(loaded_theme));
    if (!datalab_test_write_text_file(custom_theme_path,
                                      "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 300\n") ||
        !datalab_test_assert(!datalab_runtime_prefs_load_custom_theme(&loaded_theme),
                             "out-of-range custom theme should be rejected") ||
        !datalab_test_assert(loaded_theme.clear_r == 0x5au,
                             "rejected custom theme must not update caller state")) {
        datalab_test_restore_cwd(previous_cwd);
        return 0;
    }
    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(1, argv, &runtime) == 0,
                            "bootstrap failed for malformed authoring prefs") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0,
                            "config load failed for malformed authoring prefs") &&
        datalab_test_assert(runtime.workspace_authoring_profile_surface_ratio > 0.71f &&
                            runtime.workspace_authoring_profile_surface_ratio < 0.73f,
                            "malformed authoring ratio must not replace the runtime default") &&
        datalab_test_assert(!datalab_runtime_prefs_save_theme_preset_id(255u),
                            "out-of-range authoring theme must be rejected")) {
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

static int test_authoring_draft_does_not_cross_runtime_handoff(void) {
    DatalabAppRuntime runtime;
    DatalabAppState state;
    datalab_app_runtime_init(&runtime);
    runtime.text_zoom_step = 1;
    runtime.workspace_authoring_profile_surface_ratio = 0.50f;
    datalab_app_state_init(&state, "fixture.pack", DATALAB_PROFILE_IMAGE);
    state.text_zoom_step = 2;
    if (!datalab_workspace_authoring_projection_set_profile_surface_ratio(&state, 0.60f)) {
        return 0;
    }
    datalab_workspace_authoring_capture_entry_snapshot(&state);
    datalab_workspace_authoring_begin_takeover(&state);
    state.text_zoom_step = 5;
    if (!datalab_workspace_authoring_projection_apply_drag(&state, 100.0f, 1000.0f)) {
        return 0;
    }

    datalab_runtime_copy_from_app_state(&runtime, &state);
    if (!datalab_test_assert(runtime.text_zoom_step == 1 &&
                                 runtime.workspace_authoring_profile_surface_ratio > 0.49f &&
                                 runtime.workspace_authoring_profile_surface_ratio < 0.51f,
                             "active authoring drafts must not cross into accepted runtime state")) {
        return 0;
    }

    datalab_workspace_authoring_shutdown(&state);
    datalab_runtime_copy_from_app_state(&runtime, &state);
    return datalab_test_assert(runtime.text_zoom_step == 2 &&
                                   runtime.workspace_authoring_profile_surface_ratio > 0.59f &&
                                   runtime.workspace_authoring_profile_surface_ratio < 0.61f,
                               "shutdown must restore the entry baseline before runtime handoff");
}

static int test_recipe_runtime_isolation_contract(void) {
    const char *runtime_root = getenv("DATALAB_TEST_RUNTIME_ROOT");
    const char *source_root = getenv("DATALAB_TEST_SOURCE_ROOT");
    char cwd[PATH_MAX];
    struct stat source_info;
    return datalab_test_assert(runtime_root &&
                                   strncmp(runtime_root, "/private/tmp/datalab-app-contract.",
                                           strlen("/private/tmp/datalab-app-contract.")) == 0,
                               "app contract must run from an explicit private temporary runtime") &&
           datalab_test_assert(getcwd(cwd, sizeof(cwd)) && strcmp(cwd, runtime_root) == 0,
                               "app contract must never fall back to the source cwd") &&
           datalab_test_assert(source_root && stat(source_root, &source_info) == 0 && S_ISDIR(source_info.st_mode),
                               "app contract must retain an explicit source-root context");
}

int main(void) {
    char temp_dir[PATH_MAX];
    const char *default_pack = DATALAB_TEST_DEFAULT_PACK;

    if (!datalab_test_make_temp_dir(temp_dir, sizeof(temp_dir))) {
        fprintf(stderr, "contract: failed to create temp dir\n");
        return 1;
    }

    if (!test_recipe_runtime_isolation_contract()) {
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
    if (!test_png_tag_metadata(temp_dir)) {
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
    if (!test_runtime_seed_ignores_uninitialized_app_state()) {
        return 1;
    }
    if (!test_last_opened_artifact_restores_or_fails_safe(temp_dir)) {
        return 1;
    }
    if (!test_async_decode_generation_cancellation_contract(temp_dir)) {
        return 1;
    }
    if (!test_picker_surface_does_not_autoload_last_file(temp_dir)) {
        return 1;
    }
    if (!test_selected_input_root_reopens_with_its_artifacts(temp_dir)) {
        return 1;
    }
    if (!test_supported_file_catalog_natural_order_and_overflow(temp_dir)) {
        return 1;
    }
    if (!test_retained_input_catalog_refresh_contract(temp_dir)) {
        return 1;
    }
    if (!test_runtime_subsystems_seed_retained_catalog_before_first_render(temp_dir)) {
        return 1;
    }
    if (!test_recent_input_root_mru_is_unique_and_persists(temp_dir)) {
        return 1;
    }
    if (!test_theme_preset_persists_across_config_reload(temp_dir)) {
        return 1;
    }
    if (!test_workspace_projection_persists_across_config_reload(temp_dir)) {
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
    if (!test_authoring_prefs_are_atomic_and_fail_closed(temp_dir)) {
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
    if (!test_authoring_draft_does_not_cross_runtime_handoff()) {
        return 1;
    }

    puts("datalab app contract test passed");
    return 0;
}
