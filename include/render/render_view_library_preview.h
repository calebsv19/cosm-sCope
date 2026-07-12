#ifndef DATALAB_RENDER_VIEW_LIBRARY_PREVIEW_H
#define DATALAB_RENDER_VIEW_LIBRARY_PREVIEW_H

#include <stdint.h>

#include <SDL2/SDL.h>

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
    char source_path[DATALAB_APP_PATH_CAP];
    uint32_t width;
    uint32_t height;
    uint8_t image_ready;
    uint8_t image_unavailable;
} DatalabLibraryPreview;

void datalab_library_preview_init(DatalabLibraryPreview *preview);
void datalab_library_preview_destroy(DatalabLibraryPreview *preview);
void datalab_library_preview_prepare(SDL_Renderer *renderer,
                                     DatalabLibraryPreview *preview,
                                     const char *path);
void datalab_library_preview_draw(SDL_Renderer *renderer,
                                  const DatalabLibraryPreview *preview,
                                  const SDL_Rect *rect,
                                  const DatalabLibraryPreviewColors *colors,
                                  const char *label);

#endif
