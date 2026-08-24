#include "render/datalab_native_image_present.h"

#include <stdlib.h>
#include <string.h>

static int datalab_native_image_shadow_store(DatalabNativeImagePresent *present,
                                             const SDL_Surface *surface,
                                             uint32_t width,
                                             uint32_t height) {
    size_t row_bytes;
    size_t total_bytes;
    uint8_t *resized;
    uint32_t row;
    if (!present || !surface || !surface->pixels || width == 0u || height == 0u ||
        (size_t)width > SIZE_MAX / 4u) {
        return 0;
    }
    row_bytes = (size_t)width * 4u;
    if ((size_t)surface->pitch < row_bytes || (size_t)height > SIZE_MAX / row_bytes) {
        return 0;
    }
    total_bytes = row_bytes * (size_t)height;
    if (present->overlay_shadow_size != total_bytes) {
        resized = (uint8_t *)realloc(present->overlay_shadow, total_bytes);
        if (!resized) {
            return 0;
        }
        present->overlay_shadow = resized;
        present->overlay_shadow_size = total_bytes;
    }
    for (row = 0u; row < height; ++row) {
        memcpy(present->overlay_shadow + ((size_t)row * row_bytes),
               (const uint8_t *)surface->pixels + ((size_t)row * (size_t)surface->pitch),
               row_bytes);
    }
    present->overlay_width = width;
    present->overlay_height = height;
    return 1;
}

static uint64_t datalab_native_image_rgba_bytes(uint32_t width, uint32_t height) {
    return (uint64_t)width * (uint64_t)height * 4u;
}

VkResult datalab_native_image_present_prepare(DatalabNativeImagePresent *present,
                                              VkRenderer *renderer,
                                              const void *pixels,
                                              uint32_t width,
                                              uint32_t height,
                                              uint64_t content_generation,
                                              int sampling_mode,
                                              const SDL_Rect *destination,
                                              int checkerboard_enabled) {
    DatalabNativeImageIdentity requested = {
        width, height, content_generation, sampling_mode, 1
    };
    DatalabNativeImageFrameDecision decision = {0};
    VkResult result;
    if (!present || !renderer || !pixels || !destination || destination->w <= 0 ||
        destination->h <= 0 ||
        !datalab_native_image_plan_frame(&present->resident_identity,
                                         &requested,
                                         1,
                                         &decision)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (decision.upload_image) {
        vk_renderer_wait_idle(renderer);
        if (present->image_texture_initialized) {
            vk_renderer_texture_destroy(renderer, &present->image_texture);
            present->image_texture_initialized = 0;
        }
        result = vk_renderer_texture_create_from_rgba(
            renderer,
            pixels,
            width,
            height,
            sampling_mode == 1 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
            &present->image_texture);
        if (result != VK_SUCCESS) {
            memset(&present->resident_identity, 0, sizeof(present->resident_identity));
            return result;
        }
        present->image_texture_initialized = 1;
        present->resident_identity = requested;
        present->stats.image_upload_count += 1u;
        present->stats.image_upload_bytes += datalab_native_image_rgba_bytes(width, height);
    } else {
        present->stats.image_reuse_count += 1u;
    }
    present->destination = *destination;
    present->checkerboard_enabled = checkerboard_enabled ? 1 : 0;
    present->frame_active = 1;
    return VK_SUCCESS;
}

VkResult datalab_native_image_present_sync_overlay(DatalabNativeImagePresent *present,
                                                   VkRenderer *renderer,
                                                   VkRendererTexture *overlay_texture,
                                                   const SDL_Surface *overlay_surface,
                                                   uint32_t width,
                                                   uint32_t height,
                                                   int *out_uploaded,
                                                   uint64_t *out_upload_bytes) {
    int overlay_equal;
    VkResult result;
    if (out_uploaded) {
        *out_uploaded = 0;
    }
    if (out_upload_bytes) {
        *out_upload_bytes = 0u;
    }
    if (!present || !renderer || !overlay_texture || !overlay_surface ||
        !overlay_surface->pixels || width == 0u || height == 0u) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    overlay_equal = datalab_native_image_overlay_equal(present->overlay_shadow,
                                                       present->overlay_shadow_size,
                                                       present->overlay_width,
                                                       present->overlay_height,
                                                       overlay_surface->pixels,
                                                       (size_t)overlay_surface->pitch,
                                                       width,
                                                       height);
    if (overlay_equal) {
        present->stats.overlay_reuse_count += 1u;
        return VK_SUCCESS;
    }
    result = vk_renderer_texture_update_rgba_subrect(renderer,
                                                     overlay_texture,
                                                     overlay_surface->pixels,
                                                     (size_t)overlay_surface->pitch,
                                                     0u,
                                                     0u,
                                                     width,
                                                     height);
    if (result != VK_SUCCESS) {
        return result;
    }
    if (!datalab_native_image_shadow_store(present, overlay_surface, width, height)) {
        datalab_native_image_present_invalidate_overlay(present);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    present->stats.overlay_upload_count += 1u;
    present->stats.overlay_upload_bytes += datalab_native_image_rgba_bytes(width, height);
    if (out_uploaded) {
        *out_uploaded = 1;
    }
    if (out_upload_bytes) {
        *out_upload_bytes = datalab_native_image_rgba_bytes(width, height);
    }
    return VK_SUCCESS;
}

static int datalab_native_image_checker_start(int minimum, int origin, int edge) {
    int delta = minimum - origin;
    int steps = delta / edge;
    if (delta > 0 && delta % edge != 0) {
        steps += 1;
    }
    return origin + steps * edge;
}

static void datalab_native_image_draw_checkerboard(DatalabNativeImagePresent *present,
                                                   VkRenderer *renderer,
                                                   uint32_t drawable_width,
                                                   uint32_t drawable_height) {
    const int edge = 16;
    const int min_x = present->destination.x > 0 ? present->destination.x : 0;
    const int min_y = present->destination.y > 0 ? present->destination.y : 0;
    const int destination_max_x = present->destination.x + present->destination.w;
    const int destination_max_y = present->destination.y + present->destination.h;
    const int max_x = destination_max_x < (int)drawable_width
                          ? destination_max_x
                          : (int)drawable_width;
    const int max_y = destination_max_y < (int)drawable_height
                          ? destination_max_y
                          : (int)drawable_height;
    int y;
    if (!present->checkerboard_enabled || min_x >= max_x || min_y >= max_y) {
        return;
    }
    for (y = datalab_native_image_checker_start(min_y, present->destination.y, edge);
         y < max_y;
         y += edge) {
        int x;
        for (x = datalab_native_image_checker_start(min_x, present->destination.x, edge);
             x < max_x;
             x += edge) {
            SDL_Rect cell = {x, y, edge, edge};
            const int dark = ((x - present->destination.x) / edge +
                              (y - present->destination.y) / edge) & 1;
            if (cell.x + cell.w > max_x) {
                cell.w = max_x - cell.x;
            }
            if (cell.y + cell.h > max_y) {
                cell.h = max_y - cell.y;
            }
            vk_renderer_set_draw_color(renderer,
                                       dark ? 72.0f / 255.0f : 128.0f / 255.0f,
                                       dark ? 72.0f / 255.0f : 128.0f / 255.0f,
                                       dark ? 78.0f / 255.0f : 134.0f / 255.0f,
                                       1.0f);
            vk_renderer_fill_rect(renderer, &cell);
        }
    }
}

void datalab_native_image_present_draw(DatalabNativeImagePresent *present,
                                       VkRenderer *renderer,
                                       const VkRendererTexture *overlay_texture,
                                       uint32_t drawable_width,
                                       uint32_t drawable_height) {
    SDL_Rect full_drawable = {0, 0, (int)drawable_width, (int)drawable_height};
    SDL_Rect image_source;
    if (!present || !renderer || !overlay_texture || !present->frame_active ||
        !present->image_texture_initialized || drawable_width == 0u || drawable_height == 0u) {
        return;
    }
    image_source = (SDL_Rect){0,
                             0,
                             (int)present->resident_identity.width,
                             (int)present->resident_identity.height};
    vk_renderer_set_draw_color(renderer, 12.0f / 255.0f, 12.0f / 255.0f,
                               16.0f / 255.0f, 1.0f);
    vk_renderer_fill_rect(renderer, &full_drawable);
    datalab_native_image_draw_checkerboard(present,
                                           renderer,
                                           drawable_width,
                                           drawable_height);
    vk_renderer_set_draw_color(renderer, 1.0f, 1.0f, 1.0f, 1.0f);
    vk_renderer_draw_texture(renderer,
                             &present->image_texture,
                             &image_source,
                             &present->destination);
    vk_renderer_draw_texture(renderer, overlay_texture, &full_drawable, &full_drawable);
}

void datalab_native_image_present_finish_frame(DatalabNativeImagePresent *present) {
    if (present) {
        present->frame_active = 0;
    }
}

void datalab_native_image_present_invalidate_overlay(DatalabNativeImagePresent *present) {
    if (!present) {
        return;
    }
    present->overlay_width = 0u;
    present->overlay_height = 0u;
}

void datalab_native_image_present_destroy(DatalabNativeImagePresent *present,
                                          VkRenderer *renderer) {
    if (!present) {
        return;
    }
    if (renderer && present->image_texture_initialized) {
        vk_renderer_texture_destroy(renderer, &present->image_texture);
    }
    free(present->overlay_shadow);
    memset(present, 0, sizeof(*present));
}

const DatalabNativeImagePresentStats *datalab_native_image_present_stats(
    const DatalabNativeImagePresent *present) {
    return present ? &present->stats : NULL;
}
