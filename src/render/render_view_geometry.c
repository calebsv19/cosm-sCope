#include "render/render_view_internal.h"

int datalab_render_point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) {
        return 0;
    }
    return x >= rect->x && y >= rect->y &&
           x < (rect->x + rect->w) && y < (rect->y + rect->h);
}

int datalab_render_map_window_to_renderer_point(SDL_Window *window,
                                                SDL_Renderer *renderer,
                                                int window_x,
                                                int window_y,
                                                int *out_render_x,
                                                int *out_render_y) {
    int window_w = 0;
    int window_h = 0;
    int render_w = 0;
    int render_h = 0;
    if (!window || !renderer || !out_render_x || !out_render_y) {
        return 0;
    }
    SDL_GetWindowSize(window, &window_w, &window_h);
    datalab_renderer_backend_output_size(renderer, &render_w, &render_h);
    if (window_w <= 0 || window_h <= 0 || render_w <= 0 || render_h <= 0) {
        return 0;
    }
    *out_render_x = (window_x * render_w) / window_w;
    *out_render_y = (window_y * render_h) / window_h;
    return 1;
}
