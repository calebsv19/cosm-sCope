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
    return datalab_renderer_backend_map_window_to_drawable_point(window,
                                                                 renderer,
                                                                 window_x,
                                                                 window_y,
                                                                 out_render_x,
                                                                 out_render_y);
}
