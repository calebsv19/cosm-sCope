#ifndef DATALAB_RENDER_VIEW_LIBRARY_PREVIEW_H
#define DATALAB_RENDER_VIEW_LIBRARY_PREVIEW_H

#include <stdint.h>

#include <SDL2/SDL.h>

#include "app/datalab_image_residency.h"
#include "app/datalab_thumbnail_decode.h"
#include "app/app_state.h"
#include "core_base.h"

typedef struct DatalabLibraryPreviewColors {
    uint8_t fill_r, fill_g, fill_b;
    uint8_t frame_r, frame_g, frame_b;
    uint8_t text_primary_r, text_primary_g, text_primary_b;
    uint8_t text_muted_r, text_muted_g, text_muted_b;
} DatalabLibraryPreviewColors;

typedef struct DatalabLibraryPreview {
    SDL_Texture *texture;
    DatalabThumbnailDecode *decode;
    char source_path[DATALAB_APP_PATH_CAP];
    char pending_path[DATALAB_APP_PATH_CAP];
    DatalabImageIdentity displayed_identity;
    DatalabImageIdentity failed_identity;
    uint32_t pending_since_ticks;
    uint32_t width;
    uint32_t height;
    uint8_t image_ready;
    uint8_t image_unavailable;
    uint8_t image_pending;
} DatalabLibraryPreview;

enum {
    DATALAB_LIBRARY_THUMBNAIL_MAX_EDGE = 512u,
    DATALAB_LIBRARY_THUMBNAIL_MAX_BYTES = DATALAB_LIBRARY_THUMBNAIL_MAX_EDGE * DATALAB_LIBRARY_THUMBNAIL_MAX_EDGE * 4u,
    DATALAB_LIBRARY_PREVIEW_DEBOUNCE_MS = 40u
};

void datalab_library_preview_init(DatalabLibraryPreview *preview);
void datalab_library_preview_destroy(DatalabLibraryPreview *preview);
void datalab_library_preview_prepare(SDL_Renderer *renderer,
                                     DatalabLibraryPreview *preview,
                                     DatalabImageResidency *residency,
                                     const char *path,
                                     uint32_t now_ticks);
int datalab_library_preview_thumbnail_dimensions(uint32_t source_width,
                                                 uint32_t source_height,
                                                 uint32_t *out_width,
                                                 uint32_t *out_height);
int datalab_library_preview_debounce_ready(uint32_t pending_since_ticks, uint32_t now_ticks);
void datalab_library_preview_draw(SDL_Renderer *renderer,
                                  const DatalabLibraryPreview *preview,
                                  const SDL_Rect *rect,
                                  const DatalabLibraryPreviewColors *colors,
                                  const char *label);

#endif
