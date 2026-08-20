#include "render/render_view_library_preview.h"

#include <stdio.h>
#include <string.h>

#include "data/input_file_loader.h"
#include "render/render_view_internal.h"

static void datalab_library_preview_clear_texture(DatalabLibraryPreview *preview) {
    if (!preview) {
        return;
    }
    if (preview->texture) {
        SDL_DestroyTexture(preview->texture);
    }
    preview->texture = NULL;
    preview->width = 0u;
    preview->height = 0u;
    preview->image_ready = 0u;
    preview->image_unavailable = 0u;
    preview->image_pending = 0u;
}

int datalab_library_preview_thumbnail_dimensions(uint32_t source_width,
                                                 uint32_t source_height,
                                                 uint32_t *out_width,
                                                 uint32_t *out_height) {
    uint32_t edge = 0u;
    if (!out_width || !out_height || source_width == 0u || source_height == 0u) return 0;
    edge = source_width > source_height ? source_width : source_height;
    if (edge <= DATALAB_LIBRARY_THUMBNAIL_MAX_EDGE) {
        *out_width = source_width;
        *out_height = source_height;
    } else {
        *out_width = (uint32_t)(((uint64_t)source_width * DATALAB_LIBRARY_THUMBNAIL_MAX_EDGE + edge / 2u) / edge);
        *out_height = (uint32_t)(((uint64_t)source_height * DATALAB_LIBRARY_THUMBNAIL_MAX_EDGE + edge / 2u) / edge);
        if (*out_width == 0u) *out_width = 1u;
        if (*out_height == 0u) *out_height = 1u;
    }
    return datalab_image_rgba_bytes(*out_width, *out_height) <= DATALAB_LIBRARY_THUMBNAIL_MAX_BYTES;
}

int datalab_library_preview_debounce_ready(uint32_t pending_since_ticks, uint32_t now_ticks) {
    return (uint32_t)(now_ticks - pending_since_ticks) >= DATALAB_LIBRARY_PREVIEW_DEBOUNCE_MS;
}

static int datalab_library_preview_adopt_texture(SDL_Renderer *renderer,
                                                 DatalabLibraryPreview *preview,
                                                 const DatalabThumbnailResidencySlot *thumbnail,
                                                 const DatalabImageIdentity *identity) {
    SDL_Texture *texture = NULL;
    if (!renderer || !preview || !thumbnail || !thumbnail->rgba || thumbnail->width == 0u || thumbnail->height == 0u || !identity) return 0;
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, (int)thumbnail->width, (int)thumbnail->height);
    if (!texture || SDL_UpdateTexture(texture, NULL, thumbnail->rgba, (int)(thumbnail->width * 4u)) != 0) {
        if (texture) SDL_DestroyTexture(texture);
        return 0;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    if (preview->texture) SDL_DestroyTexture(preview->texture);
    preview->texture = texture;
    preview->width = thumbnail->width;
    preview->height = thumbnail->height;
    preview->displayed_identity = *identity;
    preview->image_ready = 1u;
    preview->image_unavailable = 0u;
    preview->image_pending = 0u;
    snprintf(preview->source_path, sizeof(preview->source_path), "%s", identity->canonical_path);
    return 1;
}

void datalab_library_preview_init(DatalabLibraryPreview *preview) {
    if (!preview) {
        return;
    }
    memset(preview, 0, sizeof(*preview));
    preview->decode = datalab_thumbnail_decode_create();
}

void datalab_library_preview_destroy(DatalabLibraryPreview *preview) {
    if (!preview) {
        return;
    }
    datalab_thumbnail_decode_destroy(preview->decode);
    preview->decode = NULL;
    datalab_library_preview_clear_texture(preview);
    preview->source_path[0] = '\0';
    preview->pending_path[0] = '\0';
}

void datalab_library_preview_prepare(SDL_Renderer *renderer,
                                     DatalabLibraryPreview *preview,
                                     DatalabImageResidency *residency,
                                     const char *path,
                                     uint32_t now_ticks) {
    DatalabThumbnailDecodeCompletion *completion = NULL;
    DatalabImageIdentity identity = {0};
    const DatalabThumbnailResidencySlot *thumbnail = NULL;
    if (!renderer || !preview || !residency || !path) {
        return;
    }
    if (path[0] == '\0' || (!datalab_input_file_is_png(path) && !datalab_input_file_is_bmp(path))) {
        if (preview->source_path[0] == '\0') datalab_library_preview_clear_texture(preview);
        datalab_thumbnail_decode_cancel(preview->decode);
        preview->pending_path[0] = '\0';
        return;
    }
    if (strcmp(preview->pending_path, path) != 0) {
        snprintf(preview->pending_path, sizeof(preview->pending_path), "%s", path);
        preview->pending_since_ticks = now_ticks;
        preview->image_pending = 1u;
        preview->image_unavailable = 0u;
        memset(&preview->failed_identity, 0, sizeof(preview->failed_identity));
        datalab_thumbnail_decode_cancel(preview->decode);
        return;
    }
    if (preview->image_ready && !datalab_image_residency_identity_is_current(&preview->displayed_identity)) {
        preview->pending_since_ticks = now_ticks;
        preview->image_pending = 1u;
    }
    if (!datalab_image_identity_from_path(path, &identity)) {
        preview->image_pending = 0u;
        preview->image_unavailable = 1u;
        return;
    }
    completion = datalab_thumbnail_decode_take_current(preview->decode);
    if (completion) {
        if (datalab_image_identity_equal(&completion->identity, &identity) && completion->result.code == CORE_OK &&
            completion->rgba && datalab_image_residency_admit_thumbnail_pixels(residency,
                                                                               &completion->identity,
                                                                               completion->rgba,
                                                                               completion->width,
                                                                               completion->height)) {
            completion->rgba = NULL;
            thumbnail = datalab_image_residency_find_thumbnail(residency, &identity);
            if (thumbnail && datalab_library_preview_adopt_texture(renderer, preview, thumbnail, &identity)) {
                datalab_thumbnail_decode_completion_destroy(completion);
                return;
            }
        }
        if (datalab_image_identity_equal(&completion->identity, &identity)) {
            preview->image_unavailable = 1u;
            preview->image_pending = 0u;
            preview->failed_identity = identity;
        }
        datalab_thumbnail_decode_completion_destroy(completion);
    }
    if (preview->image_unavailable && datalab_image_identity_equal(&preview->failed_identity, &identity)) return;
    if (preview->image_ready && datalab_image_identity_equal(&preview->displayed_identity, &identity)) {
        preview->image_pending = 0u;
        return;
    }
    thumbnail = datalab_image_residency_find_thumbnail(residency, &identity);
    if (thumbnail && datalab_library_preview_adopt_texture(renderer, preview, thumbnail, &identity)) return;
    if (!datalab_library_preview_debounce_ready(preview->pending_since_ticks, now_ticks)) return;
    if (!preview->decode || !datalab_thumbnail_decode_request(preview->decode,
                                                              identity.canonical_path,
                                                              DATALAB_LIBRARY_THUMBNAIL_MAX_EDGE,
                                                              DATALAB_LIBRARY_THUMBNAIL_MAX_BYTES)) {
        preview->image_unavailable = 1u;
        preview->image_pending = 0u;
        preview->failed_identity = identity;
    }
}

void datalab_library_preview_draw(SDL_Renderer *renderer,
                                  const DatalabLibraryPreview *preview,
                                  const SDL_Rect *rect,
                                  const DatalabLibraryPreviewColors *colors,
                                  const char *label) {
    SDL_Rect image_rect;
    char dimensions[64];
    int available_w = 0;
    int available_h = 0;
    float scale = 1.0f;
    if (!renderer || !rect || !colors || rect->w <= 0 || rect->h <= 0) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, colors->fill_r, colors->fill_g, colors->fill_b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, colors->frame_r, colors->frame_g, colors->frame_b, 255);
    SDL_RenderDrawRect(renderer, rect);
    draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), rect->y + datalab_scaled_px(7.0f),
                  label ? label : "IMAGE PREVIEW", 1,
                  colors->text_primary_r, colors->text_primary_g, colors->text_primary_b, 255);
    if (!preview || !preview->image_ready || !preview->texture || preview->width == 0u || preview->height == 0u) {
        draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), rect->y + rect->h / 2,
                      preview && preview->image_pending ? "PREVIEW PENDING" :
                      preview && preview->image_unavailable ? "PREVIEW UNAVAILABLE/CORRUPT" : "NO IMAGE IN THIS LOCATION",
                      1, colors->text_muted_r, colors->text_muted_g, colors->text_muted_b, 255);
        return;
    }
    available_w = rect->w - datalab_scaled_px(16.0f);
    available_h = rect->h - datalab_scaled_px(44.0f);
    if (available_w <= 0 || available_h <= 0) {
        return;
    }
    scale = (float)available_w / (float)preview->width;
    if (((float)preview->height * scale) > (float)available_h) {
        scale = (float)available_h / (float)preview->height;
    }
    image_rect.w = (int)((float)preview->width * scale);
    image_rect.h = (int)((float)preview->height * scale);
    image_rect.x = rect->x + (rect->w - image_rect.w) / 2;
    image_rect.y = rect->y + datalab_scaled_px(25.0f) + (available_h - image_rect.h) / 2;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &image_rect);
    SDL_RenderCopy(renderer, preview->texture, NULL, &image_rect);
    if (preview->image_pending) {
        draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), rect->y + datalab_scaled_px(25.0f),
                      "UPDATING PREVIEW...", 1, colors->text_muted_r, colors->text_muted_g, colors->text_muted_b, 255);
    }
    snprintf(dimensions, sizeof(dimensions), "%ux%u RGBA", preview->width, preview->height);
    draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), rect->y + rect->h - datalab_scaled_px(15.0f),
                  dimensions, 1, colors->text_muted_r, colors->text_muted_g, colors->text_muted_b, 255);
}
