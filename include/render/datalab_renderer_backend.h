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
int datalab_renderer_backend_map_point_between_extents(int source_width,
                                                       int source_height,
                                                       int destination_width,
                                                       int destination_height,
                                                       int source_x,
                                                       int source_y,
                                                       int *out_destination_x,
                                                       int *out_destination_y);
int datalab_renderer_backend_map_window_to_drawable_point(SDL_Window *window,
                                                          SDL_Renderer *renderer,
                                                          int window_x,
                                                          int window_y,
                                                          int *out_drawable_x,
                                                          int *out_drawable_y);
int datalab_renderer_backend_present(SDL_Renderer *renderer);
int datalab_renderer_backend_prepare_native_image(SDL_Renderer *renderer,
                                                  const void *pixels,
                                                  uint32_t width,
                                                  uint32_t height,
                                                  uint64_t content_generation,
                                                  uint64_t resource_generation,
                                                  int sampling_mode,
                                                  const SDL_Rect *destination,
                                                  int checkerboard_enabled);
int datalab_renderer_backend_native_image_counters(SDL_Renderer *renderer,
                                                   uint64_t *image_upload_count,
                                                   uint64_t *image_reuse_count,
                                                   uint64_t *overlay_upload_count,
                                                   uint64_t *overlay_reuse_count);
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
