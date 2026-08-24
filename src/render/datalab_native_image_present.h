#ifndef DATALAB_NATIVE_IMAGE_PRESENT_H
#define DATALAB_NATIVE_IMAGE_PRESENT_H

#include <SDL2/SDL.h>

#include <stdint.h>

#include "render/datalab_image_overlay_identity.h"
#include "render/datalab_native_image_policy.h"
#include "vk_renderer.h"

typedef struct DatalabNativeImagePresentStats {
    uint64_t image_upload_count;
    uint64_t image_upload_bytes;
    uint64_t image_reuse_count;
    uint64_t overlay_upload_count;
    uint64_t overlay_upload_bytes;
    uint64_t overlay_reuse_count;
    uint64_t overlay_redraw_count;
    uint64_t overlay_redraw_reuse_count;
} DatalabNativeImagePresentStats;

typedef struct DatalabNativeImagePresent {
    VkRendererTexture image_texture;
    DatalabNativeImageIdentity resident_identity;
    DatalabImageOverlayIdentity resident_overlay_identity;
    DatalabImageOverlayIdentity requested_overlay_identity;
    SDL_Rect destination;
    int image_texture_initialized;
    int frame_active;
    int overlay_redraw_required;
    int checkerboard_enabled;
    DatalabNativeImagePresentStats stats;
} DatalabNativeImagePresent;

VkResult datalab_native_image_present_prepare(DatalabNativeImagePresent *present,
                                              VkRenderer *renderer,
                                              const void *pixels,
                                              uint32_t width,
                                              uint32_t height,
                                              uint64_t content_generation,
                                              int sampling_mode,
                                              const SDL_Rect *destination,
                                              int checkerboard_enabled,
                                              const DatalabImageOverlayIdentity *overlay_identity,
                                              int *out_overlay_redraw_required);
VkResult datalab_native_image_present_sync_overlay(DatalabNativeImagePresent *present,
                                                   VkRenderer *renderer,
                                                   VkRendererTexture *overlay_texture,
                                                   const SDL_Surface *overlay_surface,
                                                   uint32_t width,
                                                   uint32_t height,
                                                   int *out_uploaded,
                                                   uint64_t *out_upload_bytes);
void datalab_native_image_present_draw(DatalabNativeImagePresent *present,
                                       VkRenderer *renderer,
                                       const VkRendererTexture *overlay_texture,
                                       uint32_t drawable_width,
                                       uint32_t drawable_height);
void datalab_native_image_present_finish_frame(DatalabNativeImagePresent *present);
void datalab_native_image_present_invalidate_overlay(DatalabNativeImagePresent *present);
void datalab_native_image_present_destroy(DatalabNativeImagePresent *present,
                                          VkRenderer *renderer);
const DatalabNativeImagePresentStats *datalab_native_image_present_stats(
    const DatalabNativeImagePresent *present);

#endif
