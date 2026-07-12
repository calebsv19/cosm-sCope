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
}

void datalab_library_preview_init(DatalabLibraryPreview *preview) {
    if (!preview) {
        return;
    }
    memset(preview, 0, sizeof(*preview));
}

void datalab_library_preview_destroy(DatalabLibraryPreview *preview) {
    if (!preview) {
        return;
    }
    datalab_library_preview_clear_texture(preview);
    preview->source_path[0] = '\0';
}

void datalab_library_preview_prepare(SDL_Renderer *renderer,
                                     DatalabLibraryPreview *preview,
                                     const char *path) {
    DatalabFrame frame = {0};
    CoreResult result;
    size_t row_bytes = 0u;
    if (!renderer || !preview || !path) {
        return;
    }
    if (strcmp(preview->source_path, path) == 0) {
        return;
    }
    datalab_library_preview_clear_texture(preview);
    snprintf(preview->source_path, sizeof(preview->source_path), "%s", path);
    if (path[0] == '\0' ||
        (!datalab_input_file_is_png(path) && !datalab_input_file_is_bmp(path))) {
        return;
    }
    result = datalab_load_input_file(path, &frame);
    if (result.code != CORE_OK || frame.profile != DATALAB_PROFILE_IMAGE || !frame.drawing_rgba ||
        datalab_input_image_bounds(frame.width, frame.height, &row_bytes, NULL).code != CORE_OK) {
        preview->image_unavailable = 1u;
        datalab_frame_free(&frame);
        return;
    }
    preview->texture = SDL_CreateTexture(renderer,
                                         SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC,
                                         (int)frame.width,
                                         (int)frame.height);
    if (!preview->texture ||
        SDL_UpdateTexture(preview->texture, NULL, frame.drawing_rgba, (int)row_bytes) != 0) {
        if (preview->texture) {
            SDL_DestroyTexture(preview->texture);
            preview->texture = NULL;
        }
        preview->image_unavailable = 1u;
        datalab_frame_free(&frame);
        return;
    }
    SDL_SetTextureBlendMode(preview->texture, SDL_BLENDMODE_BLEND);
    preview->width = frame.width;
    preview->height = frame.height;
    preview->image_ready = 1u;
    datalab_frame_free(&frame);
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
                      preview && preview->image_unavailable ? "PREVIEW UNAVAILABLE" : "NO IMAGE IN THIS LOCATION",
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
    snprintf(dimensions, sizeof(dimensions), "%ux%u RGBA", preview->width, preview->height);
    draw_text_5x7(renderer, rect->x + datalab_scaled_px(8.0f), rect->y + rect->h - datalab_scaled_px(15.0f),
                  dimensions, 1, colors->text_muted_r, colors->text_muted_g, colors->text_muted_b, 255);
}
