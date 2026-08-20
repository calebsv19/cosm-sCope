#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "app/datalab_thumbnail_decode.h"
#include "render/render_view_library_preview.h"

static int require(int condition, const char *message) {
    if (!condition) fprintf(stderr, "thumbnail-decode-contract: %s\n", message);
    return condition;
}

static int write_bmp(const char *path, int width, int height, uint32_t color) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return 0;
    for (int y = 0; y < height; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)surface->pixels + y * surface->pitch);
        for (int x = 0; x < width; ++x) row[x] = color;
    }
    if (SDL_SaveBMP(surface, path) != 0) {
        SDL_FreeSurface(surface);
        return 0;
    }
    SDL_FreeSurface(surface);
    return 1;
}

static DatalabThumbnailDecodeCompletion *wait_for(DatalabThumbnailDecode *decode,
                                                   const char *path) {
    for (int attempt = 0; attempt < 1000; ++attempt) {
        DatalabThumbnailDecodeCompletion *completion = datalab_thumbnail_decode_take_current(decode);
        if (completion) return completion;
        (void)datalab_thumbnail_decode_request(decode, path, 512u, 512u * 512u * 4u);
        SDL_Delay(1u);
    }
    return NULL;
}

int main(void) {
    char root_template[] = "/private/tmp/datalab-thumbnail-decode.XXXXXX";
    char first_path[DATALAB_APP_PATH_CAP];
    char second_path[DATALAB_APP_PATH_CAP];
    char *root = mkdtemp(root_template);
    DatalabThumbnailDecode *decode = NULL;
    DatalabThumbnailDecodeCompletion *completion = NULL;
    DatalabThumbnailDecodeStats stats;
    SDL_Surface *preview_surface = NULL;
    SDL_Renderer *preview_renderer = NULL;
    DatalabImageResidency residency = {0};
    DatalabLibraryPreview preview = {0};
    int ok = 0;
    if (!root ||
        snprintf(first_path, sizeof(first_path), "%s/first.bmp", root) >= (int)sizeof(first_path) ||
        snprintf(second_path, sizeof(second_path), "%s/second.bmp", root) >= (int)sizeof(second_path) ||
        !write_bmp(first_path, 1024, 512, 0xff0000ffu) ||
        !write_bmp(second_path, 800, 1200, 0xff00ff00u)) return 1;

    decode = datalab_thumbnail_decode_create();
    if (!require(decode != NULL, "bounded worker must initialize") ||
        !require(datalab_thumbnail_decode_request(decode, first_path, 512u, 512u * 512u * 4u),
                 "first thumbnail request must enqueue")) goto cleanup;
    completion = wait_for(decode, first_path);
    if (!require(completion && completion->result.code == CORE_OK && completion->rgba &&
                     completion->width == 512u && completion->height == 256u,
                 "worker must decode and downscale to the bounded aspect-preserving preview")) goto cleanup;
    datalab_thumbnail_decode_completion_destroy(completion);
    completion = NULL;

    if (!require(datalab_thumbnail_decode_request(decode, first_path, 512u, 512u * 512u * 4u) &&
                     datalab_thumbnail_decode_request(decode, second_path, 512u, 512u * 512u * 4u),
                 "rapid latest-wins requests must not block or fail while one decode is active")) goto cleanup;
    completion = wait_for(decode, second_path);
    if (!require(completion && completion->result.code == CORE_OK && completion->rgba &&
                     strcmp(completion->identity.canonical_path, second_path) == 0 &&
                     completion->width == 341u && completion->height == 512u,
                 "rapid selection must converge on only the newest requested thumbnail")) goto cleanup;
    stats = datalab_thumbnail_decode_stats(decode);
    if (!require(stats.peak_outstanding == 1u && stats.selection_count >= 2u &&
                     stats.submitted_count >= 2u && stats.completed_count >= 2u,
                 "thumbnail work and retained results must remain single-flight and bounded")) goto cleanup;
    datalab_thumbnail_decode_completion_destroy(completion);
    completion = NULL;

    preview_surface = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32);
    preview_renderer = preview_surface ? SDL_CreateSoftwareRenderer(preview_surface) : NULL;
    datalab_image_residency_init(&residency);
    datalab_library_preview_init(&preview);
    if (!require(preview_surface && preview_renderer && preview.decode,
                 "render-thread preview integration must initialize")) goto cleanup;
    datalab_library_preview_prepare(preview_renderer, &preview, &residency, first_path, 100u);
    datalab_library_preview_prepare(preview_renderer, &preview, &residency, first_path, 140u);
    for (int attempt = 0; attempt < 1000 && !preview.image_ready; ++attempt) {
        SDL_Delay(1u);
        datalab_library_preview_prepare(preview_renderer, &preview, &residency, first_path, 141u + (uint32_t)attempt);
    }
    if (!require(preview.image_ready && preview.texture && preview.width == 512u && preview.height == 256u,
                 "picker integration must asynchronously admit and upload the bounded thumbnail")) goto cleanup;
    datalab_library_preview_prepare(preview_renderer, &preview, &residency, second_path, 2000u);
    if (!require(preview.image_ready && preview.image_pending && preview.texture,
                 "new selection must retain the last good texture while replacement is pending")) goto cleanup;
    ok = 1;

cleanup:
    datalab_library_preview_destroy(&preview);
    datalab_image_residency_destroy(&residency);
    SDL_DestroyRenderer(preview_renderer);
    SDL_FreeSurface(preview_surface);
    datalab_thumbnail_decode_completion_destroy(completion);
    datalab_thumbnail_decode_destroy(decode);
    (void)unlink(first_path);
    (void)unlink(second_path);
    if (root) (void)rmdir(root);
    if (ok) puts("datalab thumbnail decode contract test passed");
    return ok ? 0 : 1;
}
