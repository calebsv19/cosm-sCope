#include "render/render_view_internal.h"

#include <math.h>
#include <string.h>

static int datalab_raster_viewport_scaled_dim(uint32_t content_dim, float zoom) {
    float scaled = (float)content_dim * zoom;
    int rounded = 0;
    if (scaled < 1.0f) {
        return 1;
    }
    rounded = (int)lroundf(scaled);
    return rounded > 0 ? rounded : 1;
}

void datalab_raster_viewport_derive_frame(SDL_Renderer *renderer,
                                          const DatalabFrame *frame,
                                          DatalabAppState *app_state,
                                          DatalabSketchRenderDeriveFrame *out_derive) {
    int view_width = 0;
    int view_height = 0;
    DatalabRasterViewportState *state = NULL;
    if (!renderer || !frame || !app_state || !out_derive) {
        return;
    }
    datalab_renderer_backend_output_size(renderer, &view_width, &view_height);
    if (view_width <= 0 || view_height <= 0) {
        calc_fit_rect(1, 1, frame->width, frame->height, &out_derive->dst);
        out_derive->zoom = 1.0f;
        out_derive->fit_mode = 1u;
        return;
    }
    state = &app_state->raster_viewport;
    datalab_raster_viewport_sync_state(state, view_width, view_height, frame->width, frame->height);
    if (!state->valid) {
        calc_fit_rect(view_width, view_height, frame->width, frame->height, &out_derive->dst);
        out_derive->zoom = 1.0f;
        out_derive->fit_mode = 1u;
        return;
    }
    out_derive->dst.x = (int)lroundf(state->viewport.pan_x);
    out_derive->dst.y = (int)lroundf(state->viewport.pan_y);
    out_derive->dst.w = datalab_raster_viewport_scaled_dim(frame->width, state->viewport.zoom);
    out_derive->dst.h = datalab_raster_viewport_scaled_dim(frame->height, state->viewport.zoom);
    out_derive->zoom = state->viewport.zoom;
    out_derive->fit_mode = state->fit_mode ? 1u : 0u;
}
