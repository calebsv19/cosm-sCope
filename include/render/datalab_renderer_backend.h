#ifndef DATALAB_RENDERER_BACKEND_H
#define DATALAB_RENDERER_BACKEND_H

#include <SDL2/SDL.h>

typedef enum DatalabRendererBackendKind {
    DATALAB_RENDERER_BACKEND_SDL = 0,
    DATALAB_RENDERER_BACKEND_VULKAN = 1
} DatalabRendererBackendKind;

uint32_t datalab_renderer_backend_window_flags(void);
SDL_Renderer *datalab_renderer_backend_create(SDL_Window *window);
void datalab_renderer_backend_destroy(SDL_Renderer *renderer);
DatalabRendererBackendKind datalab_renderer_backend_kind(SDL_Renderer *renderer);
int datalab_renderer_backend_output_size(SDL_Renderer *renderer, int *width, int *height);
int datalab_renderer_backend_present(SDL_Renderer *renderer);
int datalab_renderer_backend_request_capture(SDL_Renderer *renderer, const char *path);
int datalab_renderer_backend_verify(SDL_Renderer *renderer,
                                    const char *stage,
                                    int require_validation);
int datalab_renderer_backend_drawable_metrics(SDL_Renderer *renderer,
                                              int *logical_width,
                                              int *logical_height,
                                              int *drawable_width,
                                              int *drawable_height,
                                              double *scale);

#endif
