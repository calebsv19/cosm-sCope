#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "app/datalab_async_decode.h"
#include "app/datalab_runtime_pack.h"
#include "datalab/datalab_app_main.h"
#include "data/input_file_loader.h"

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "async-decode-contract: %s\n", message);
    }
    return condition;
}

static int write_tiny_bmp(const char *path, uint32_t color) {
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
    pixels[0] = color;
    pixels[1] = color;
    pixels[pitch_pixels] = color;
    pixels[pitch_pixels + 1] = color;
    if (SDL_SaveBMP(surface, path) != 0) {
        SDL_FreeSurface(surface);
        return 0;
    }
    SDL_FreeSurface(surface);
    return 1;
}

static int wait_for_pending(DatalabAppRuntime *runtime, DatalabAppState *state) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        (void)datalab_async_decode_pump(&runtime->async_decode, runtime, state);
        if (datalab_async_decode_pending_selected(&runtime->async_decode)) {
            return 1;
        }
        SDL_Delay(2u);
    }
    return 0;
}

static int wait_for_failure(DatalabAppRuntime *runtime, DatalabAppState *state) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        (void)datalab_async_decode_pump(&runtime->async_decode, runtime, state);
        if (runtime->last_load_error[0] != '\0') {
            return 1;
        }
        SDL_Delay(2u);
    }
    return 0;
}

int main(void) {
    char root_template[] = "/private/tmp/datalab-async-contract.XXXXXX";
    char first_path[DATALAB_APP_PATH_CAP];
    char second_path[DATALAB_APP_PATH_CAP];
    char third_path[DATALAB_APP_PATH_CAP];
    char fourth_path[DATALAB_APP_PATH_CAP];
    char corrupt_path[DATALAB_APP_PATH_CAP];
    char *root = mkdtemp(root_template);
    DatalabAppRuntime runtime;
    DatalabAppState state;
    CoreResult load_result;
    uint8_t *first_pixels = NULL;
    int ok = 0;

    if (!root ||
        snprintf(first_path, sizeof(first_path), "%s/frame_0000.bmp", root) >= (int)sizeof(first_path) ||
        snprintf(second_path, sizeof(second_path), "%s/frame_0001.bmp", root) >= (int)sizeof(second_path) ||
        snprintf(third_path, sizeof(third_path), "%s/frame_0002.bmp", root) >= (int)sizeof(third_path) ||
        snprintf(fourth_path, sizeof(fourth_path), "%s/frame_0003.bmp", root) >= (int)sizeof(fourth_path) ||
        snprintf(corrupt_path, sizeof(corrupt_path), "%s/frame_0004.bmp", root) >= (int)sizeof(corrupt_path) ||
        !write_tiny_bmp(first_path, 0xff0000ffu) || !write_tiny_bmp(second_path, 0xff00ff00u) ||
        !write_tiny_bmp(third_path, 0xffff0000u) || !write_tiny_bmp(fourth_path, 0xffffffffu)) {
        return 1;
    }
    {
        FILE *corrupt = fopen(corrupt_path, "wb");
        if (!corrupt || fputs("not a bmp", corrupt) < 0 || fclose(corrupt) != 0) {
            return 1;
        }
    }
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        return 1;
    }

    datalab_app_runtime_init(&runtime);
    load_result = datalab_load_input_file(first_path, &runtime.frame);
    if (load_result.code != CORE_OK) {
        goto cleanup;
    }
    runtime.frame_loaded = 1;
    runtime.pack_path = first_path;
    snprintf(runtime.selected_pack_path, sizeof(runtime.selected_pack_path), "%s", first_path);
    datalab_runtime_note_active_raster_content(&runtime);
    first_pixels = runtime.frame.drawing_rgba;
    datalab_app_state_init(&state, first_path, DATALAB_PROFILE_IMAGE);
    state.runtime_owner = &runtime;
    state.async_decode = &runtime.async_decode;
    if (!datalab_async_decode_start(&runtime.async_decode)) {
        goto cleanup;
    }

    if (!require(datalab_async_decode_request_selected(&runtime.async_decode, second_path),
                 "second frame request should enqueue") ||
        !require(wait_for_pending(&runtime, &state),
                 "second frame should decode into a pending candidate") ||
        !require(runtime.frame.drawing_rgba == first_pixels && strcmp(runtime.pack_path, first_path) == 0,
                 "pending decode must retain the displayed CPU/GPU source frame") ||
        !require(datalab_async_decode_pending_selected(&runtime.async_decode)->generation ==
                     datalab_async_decode_current_generation(&runtime.async_decode),
                 "pending candidate must belong to the current generation")) {
        goto cleanup;
    }

    datalab_async_decode_reject_pending_selected(&runtime.async_decode, &runtime, &state, "forced upload failure");
    if (!require(runtime.frame.drawing_rgba == first_pixels && runtime.frame_loaded &&
                 strstr(runtime.last_load_error, "keeping current frame") != NULL,
                 "failed staged upload must preserve last-known-good frame")) {
        goto cleanup;
    }

    if (!require(datalab_async_decode_request_selected(&runtime.async_decode, second_path) &&
                 wait_for_pending(&runtime, &state) &&
                 datalab_async_decode_commit_pending_selected(&runtime.async_decode, &runtime, &state) &&
                 strcmp(runtime.pack_path, second_path) == 0 && runtime.frame_loaded,
                 "successful current candidate must swap only at explicit render-thread commit")) {
        goto cleanup;
    }

    if (!require(datalab_async_decode_request_selected(&runtime.async_decode, third_path) &&
                 wait_for_pending(&runtime, &state) &&
                 datalab_async_decode_request_selected(&runtime.async_decode, fourth_path) &&
                 runtime.frame_loaded && strcmp(runtime.pack_path, second_path) == 0 &&
                 wait_for_pending(&runtime, &state) &&
                 datalab_async_decode_commit_pending_selected(&runtime.async_decode, &runtime, &state) &&
                 strcmp(runtime.pack_path, fourth_path) == 0,
                 "stale/cancelled candidates must not clear or overwrite the displayed frame")) {
        goto cleanup;
    }

    if (!require(datalab_async_decode_request_selected(&runtime.async_decode, third_path) &&
                 datalab_async_decode_request_selected(&runtime.async_decode, first_path) &&
                 datalab_async_decode_request_selected(&runtime.async_decode, second_path) &&
                 wait_for_pending(&runtime, &state) &&
                 datalab_async_decode_commit_pending_selected(&runtime.async_decode, &runtime, &state) &&
                 strcmp(runtime.pack_path, second_path) == 0 &&
                 runtime.async_decode.selected_request_count >= 7u &&
                 runtime.async_decode.selected_stale_discard_count > 0u,
                 "rapid navigation must converge on the newest request without a blank intermediate state")) {
        goto cleanup;
    }

    if (!require(datalab_async_decode_request_selected(&runtime.async_decode, corrupt_path) &&
                 wait_for_failure(&runtime, &state) && runtime.frame_loaded &&
                 strcmp(runtime.pack_path, second_path) == 0 &&
                 strstr(runtime.last_load_error, "keeping current frame") != NULL,
                 "decode failure must retain the last-known-good frame")) {
        goto cleanup;
    }
    ok = 1;

cleanup:
    datalab_app_shutdown(&runtime);
    SDL_Quit();
    (void)unlink(first_path);
    (void)unlink(second_path);
    (void)unlink(third_path);
    (void)unlink(fourth_path);
    (void)unlink(corrupt_path);
    if (root) {
        (void)rmdir(root);
    }
    if (ok) {
        puts("datalab async decode contract test passed");
    }
    return ok ? 0 : 1;
}
