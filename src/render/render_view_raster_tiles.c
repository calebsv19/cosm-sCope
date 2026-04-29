#include "render/render_view_internal.h"

#include <math.h>
#include <string.h>

enum {
    DATALAB_RASTER_TILE_EDGE_DEFAULT = 1024,
    DATALAB_RASTER_TILE_CACHE_MIN = 12,
    DATALAB_RASTER_TILE_CACHE_MAX = 48,
    DATALAB_RASTER_TILE_PREFETCH_RADIUS_DEFAULT = 1
};

static int datalab_raster_tile_clamp_edge(int max_texture_width, int max_texture_height) {
    int edge = DATALAB_RASTER_TILE_EDGE_DEFAULT;
    if (max_texture_width > 0 && max_texture_width < edge) {
        edge = max_texture_width;
    }
    if (max_texture_height > 0 && max_texture_height < edge) {
        edge = max_texture_height;
    }
    if (edge < 64) {
        edge = 64;
    }
    return edge;
}

static int datalab_raster_tile_cache_clamp_capacity(int capacity) {
    if (capacity < DATALAB_RASTER_TILE_CACHE_MIN) {
        return DATALAB_RASTER_TILE_CACHE_MIN;
    }
    if (capacity > DATALAB_RASTER_TILE_CACHE_MAX) {
        return DATALAB_RASTER_TILE_CACHE_MAX;
    }
    return capacity;
}

static void datalab_raster_texture_state_reset(DatalabRasterTextureState *state) {
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

static void datalab_raster_tile_cache_reset_entry(DatalabRasterTileCacheEntry *entry) {
    if (!entry) {
        return;
    }
    entry->tile_x_index = 0;
    entry->tile_y_index = 0;
    entry->tile_w = 0;
    entry->tile_h = 0;
    entry->stamp = 0u;
    entry->valid = 0;
}

static void datalab_raster_texture_state_note_renderer_limits(SDL_Renderer *renderer,
                                                              DatalabRasterTextureState *state) {
    SDL_RendererInfo info;
    if (!renderer || !state) {
        return;
    }
    memset(&info, 0, sizeof(info));
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        state->max_texture_width = (int)info.max_texture_width;
        state->max_texture_height = (int)info.max_texture_height;
    }
}

static int datalab_raster_texture_state_needs_tiling(const DatalabRasterTextureState *state,
                                                     uint32_t content_width,
                                                     uint32_t content_height) {
    if (!state) {
        return 1;
    }
    if (state->max_texture_width > 0 && (int)content_width > state->max_texture_width) {
        return 1;
    }
    if (state->max_texture_height > 0 && (int)content_height > state->max_texture_height) {
        return 1;
    }
    return 0;
}

static int datalab_raster_tile_cache_target_capacity(SDL_Renderer *renderer, int tile_edge) {
    int view_width = 0;
    int view_height = 0;
    int cols = 0;
    int rows = 0;
    int capacity = 0;
    if (!renderer || tile_edge <= 0) {
        return DATALAB_RASTER_TILE_CACHE_MIN;
    }
    SDL_GetRendererOutputSize(renderer, &view_width, &view_height);
    if (view_width <= 0 || view_height <= 0) {
        return DATALAB_RASTER_TILE_CACHE_MIN;
    }
    cols = (view_width + tile_edge - 1) / tile_edge;
    rows = (view_height + tile_edge - 1) / tile_edge;
    capacity = (cols + 2) * (rows + 2);
    return datalab_raster_tile_cache_clamp_capacity(capacity);
}

static int datalab_raster_tile_visible_range_start(int screen_origin, float zoom) {
    if (zoom <= 0.0f) {
        return 0;
    }
    if (screen_origin >= 0) {
        return 0;
    }
    return (int)floorf((float)(-screen_origin) / zoom);
}

static int datalab_raster_tile_visible_range_end(int screen_extent,
                                                 int screen_origin,
                                                 float zoom,
                                                 int content_extent) {
    int end = content_extent;
    if (zoom <= 0.0f) {
        return 0;
    }
    end = (int)ceilf((float)(screen_extent - screen_origin) / zoom);
    if (end < 0) {
        end = 0;
    }
    if (end > content_extent) {
        end = content_extent;
    }
    return end;
}

static void datalab_raster_tile_screen_bounds(const DatalabSketchRenderDeriveFrame *derive,
                                              int content_x,
                                              int content_y,
                                              int content_w,
                                              int content_h,
                                              SDL_Rect *out_dst) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!derive || !out_dst) {
        return;
    }
    x0 = (int)lroundf((float)derive->dst.x + ((float)content_x * derive->zoom));
    y0 = (int)lroundf((float)derive->dst.y + ((float)content_y * derive->zoom));
    x1 = (int)lroundf((float)derive->dst.x + ((float)(content_x + content_w) * derive->zoom));
    y1 = (int)lroundf((float)derive->dst.y + ((float)(content_y + content_h) * derive->zoom));
    out_dst->x = x0;
    out_dst->y = y0;
    out_dst->w = x1 - x0;
    out_dst->h = y1 - y0;
    if (out_dst->w < 1) {
        out_dst->w = 1;
    }
    if (out_dst->h < 1) {
        out_dst->h = 1;
    }
}

static void datalab_raster_texture_state_destroy_cache(DatalabRasterTextureState *state) {
    if (!state || !state->cache_entries) {
        return;
    }
    for (int i = 0; i < state->cache_capacity; ++i) {
        if (state->cache_entries[i].texture) {
            SDL_DestroyTexture(state->cache_entries[i].texture);
        }
        datalab_raster_tile_cache_reset_entry(&state->cache_entries[i]);
    }
    core_free(state->cache_entries);
    state->cache_entries = NULL;
    state->cache_capacity = 0;
    state->cache_stamp = 0u;
}

static DatalabRasterTileCacheEntry *datalab_raster_tile_cache_find(DatalabRasterTextureState *state,
                                                                   int tile_x_index,
                                                                   int tile_y_index) {
    if (!state || !state->cache_entries) {
        return NULL;
    }
    for (int i = 0; i < state->cache_capacity; ++i) {
        DatalabRasterTileCacheEntry *entry = &state->cache_entries[i];
        if (entry->valid && entry->tile_x_index == tile_x_index && entry->tile_y_index == tile_y_index) {
            return entry;
        }
    }
    return NULL;
}

static DatalabRasterTileCacheEntry *datalab_raster_tile_cache_pick_slot(DatalabRasterTextureState *state) {
    DatalabRasterTileCacheEntry *best = NULL;
    if (!state || !state->cache_entries) {
        return NULL;
    }
    for (int i = 0; i < state->cache_capacity; ++i) {
        DatalabRasterTileCacheEntry *entry = &state->cache_entries[i];
        if (!entry->texture) {
            continue;
        }
        if (!entry->valid) {
            return entry;
        }
        if (!best || entry->stamp < best->stamp) {
            best = entry;
        }
    }
    return best;
}

static CoreResult datalab_raster_tile_cache_touch(DatalabRasterTextureState *state,
                                                  const DatalabFrame *frame,
                                                  int tile_x_index,
                                                  int tile_y_index,
                                                  DatalabRasterTileCacheEntry **out_entry) {
    int tile_x = 0;
    int tile_y = 0;
    int tile_w = 0;
    int tile_h = 0;
    int pitch = 0;
    DatalabRasterTileCacheEntry *entry = NULL;
    const uint8_t *tile_pixels = NULL;
    SDL_Rect update_rect = {0, 0, 0, 0};
    if (out_entry) {
        *out_entry = NULL;
    }
    if (!state || !frame || !frame->drawing_rgba || tile_x_index < 0 || tile_y_index < 0) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid tile cache touch request" };
    }

    entry = datalab_raster_tile_cache_find(state, tile_x_index, tile_y_index);
    if (entry) {
        entry->stamp = ++state->cache_stamp;
        if (out_entry) {
            *out_entry = entry;
        }
        return core_result_ok();
    }

    entry = datalab_raster_tile_cache_pick_slot(state);
    if (!entry || !entry->texture) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "no raster tile cache slot available" };
    }

    tile_x = tile_x_index * state->tile_edge;
    tile_y = tile_y_index * state->tile_edge;
    tile_w = state->tile_edge;
    tile_h = state->tile_edge;
    if (tile_x >= (int)frame->width || tile_y >= (int)frame->height) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "tile index out of range" };
    }
    if (tile_x + tile_w > (int)frame->width) {
        tile_w = (int)frame->width - tile_x;
    }
    if (tile_y + tile_h > (int)frame->height) {
        tile_h = (int)frame->height - tile_y;
    }
    pitch = (int)frame->width * 4;
    update_rect.w = tile_w;
    update_rect.h = tile_h;
    tile_pixels = frame->drawing_rgba + (((size_t)tile_y * (size_t)frame->width) + (size_t)tile_x) * 4u;
    if (SDL_UpdateTexture(entry->texture, &update_rect, tile_pixels, pitch) != 0) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }

    entry->tile_x_index = tile_x_index;
    entry->tile_y_index = tile_y_index;
    entry->tile_w = tile_w;
    entry->tile_h = tile_h;
    entry->stamp = ++state->cache_stamp;
    entry->valid = 1;
    if (out_entry) {
        *out_entry = entry;
    }
    return core_result_ok();
}

static CoreResult datalab_raster_render_tiled_streaming(SDL_Renderer *renderer,
                                                        const DatalabFrame *frame,
                                                        const DatalabSketchRenderDeriveFrame *derive,
                                                        DatalabRasterTextureState *state,
                                                        int start_tile_x,
                                                        int start_tile_y,
                                                        int end_tile_x,
                                                        int end_tile_y) {
    int pitch = 0;
    if (!renderer || !frame || !derive || !state || !state->tile_texture || !frame->drawing_rgba) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid tiled stream render request" };
    }
    pitch = (int)frame->width * 4;
    for (int tile_y_index = start_tile_y; tile_y_index <= end_tile_y; ++tile_y_index) {
        for (int tile_x_index = start_tile_x; tile_x_index <= end_tile_x; ++tile_x_index) {
            int tile_x = tile_x_index * state->tile_edge;
            int tile_y = tile_y_index * state->tile_edge;
            int tile_w = state->tile_edge;
            int tile_h = state->tile_edge;
            SDL_Rect update_rect = {0, 0, 0, 0};
            SDL_Rect src_rect = {0, 0, 0, 0};
            SDL_Rect dst_rect = {0};
            const uint8_t *tile_pixels = NULL;

            if (tile_x + tile_w > (int)frame->width) {
                tile_w = (int)frame->width - tile_x;
            }
            if (tile_y + tile_h > (int)frame->height) {
                tile_h = (int)frame->height - tile_y;
            }
            if (tile_w <= 0 || tile_h <= 0) {
                continue;
            }

            update_rect.w = tile_w;
            update_rect.h = tile_h;
            src_rect.w = tile_w;
            src_rect.h = tile_h;
            datalab_raster_tile_screen_bounds(derive, tile_x, tile_y, tile_w, tile_h, &dst_rect);
            tile_pixels = frame->drawing_rgba + (((size_t)tile_y * (size_t)frame->width) + (size_t)tile_x) * 4u;
            if (SDL_UpdateTexture(state->tile_texture, &update_rect, tile_pixels, pitch) != 0) {
                return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
            }
            if (SDL_RenderCopy(renderer, state->tile_texture, &src_rect, &dst_rect) != 0) {
                return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
            }
        }
    }
    return core_result_ok();
}

static CoreResult datalab_raster_render_tiled(SDL_Renderer *renderer,
                                              const DatalabFrame *frame,
                                              const DatalabSketchRenderDeriveFrame *derive,
                                              DatalabRasterTextureState *state) {
    int view_width = 0;
    int view_height = 0;
    int visible_x0 = 0;
    int visible_y0 = 0;
    int visible_x1 = 0;
    int visible_y1 = 0;
    int start_tile_x = 0;
    int start_tile_y = 0;
    int end_tile_x = 0;
    int end_tile_y = 0;
    int visible_tile_count = 0;
    if (!renderer || !frame || !derive || !state || (!state->tile_texture && !state->cache_entries) || !frame->drawing_rgba) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid tiled raster render request" };
    }
    if (derive->zoom <= 0.0f || state->tile_edge <= 0) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid tiled raster viewport state" };
    }

    SDL_GetRendererOutputSize(renderer, &view_width, &view_height);
    visible_x0 = datalab_raster_tile_visible_range_start(derive->dst.x, derive->zoom);
    visible_y0 = datalab_raster_tile_visible_range_start(derive->dst.y, derive->zoom);
    visible_x1 = datalab_raster_tile_visible_range_end(view_width, derive->dst.x, derive->zoom, (int)frame->width);
    visible_y1 = datalab_raster_tile_visible_range_end(view_height, derive->dst.y, derive->zoom, (int)frame->height);
    if (visible_x0 >= visible_x1 || visible_y0 >= visible_y1) {
        return core_result_ok();
    }

    start_tile_x = visible_x0 / state->tile_edge;
    start_tile_y = visible_y0 / state->tile_edge;
    end_tile_x = (visible_x1 - 1) / state->tile_edge;
    end_tile_y = (visible_y1 - 1) / state->tile_edge;
    visible_tile_count = (end_tile_x - start_tile_x + 1) * (end_tile_y - start_tile_y + 1);
    if (!state->cache_entries || state->cache_capacity <= 0) {
        return datalab_raster_render_tiled_streaming(renderer,
                                                     frame,
                                                     derive,
                                                     state,
                                                     start_tile_x,
                                                     start_tile_y,
                                                     end_tile_x,
                                                     end_tile_y);
    }

    for (int tile_y_index = start_tile_y; tile_y_index <= end_tile_y; ++tile_y_index) {
        for (int tile_x_index = start_tile_x; tile_x_index <= end_tile_x; ++tile_x_index) {
            DatalabRasterTileCacheEntry *entry = NULL;
            SDL_Rect src_rect = {0, 0, 0, 0};
            SDL_Rect dst_rect = {0};
            CoreResult touch_r = datalab_raster_tile_cache_touch(state,
                                                                 frame,
                                                                 tile_x_index,
                                                                 tile_y_index,
                                                                 &entry);
            if (touch_r.code != CORE_OK) {
                return touch_r;
            }
            if (!entry) {
                return (CoreResult){ CORE_ERR_IO, "missing raster tile cache entry" };
            }
            src_rect.w = entry->tile_w;
            src_rect.h = entry->tile_h;
            datalab_raster_tile_screen_bounds(derive,
                                              tile_x_index * state->tile_edge,
                                              tile_y_index * state->tile_edge,
                                              entry->tile_w,
                                              entry->tile_h,
                                              &dst_rect);
            if (SDL_RenderCopy(renderer, entry->texture, &src_rect, &dst_rect) != 0) {
                return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
            }
        }
    }

    if (state->prefetch_radius > 0 && visible_tile_count < state->cache_capacity) {
        int prefetch_start_x = start_tile_x - state->prefetch_radius;
        int prefetch_start_y = start_tile_y - state->prefetch_radius;
        int prefetch_end_x = end_tile_x + state->prefetch_radius;
        int prefetch_end_y = end_tile_y + state->prefetch_radius;
        if (prefetch_start_x < 0) prefetch_start_x = 0;
        if (prefetch_start_y < 0) prefetch_start_y = 0;
        if (prefetch_end_x > ((int)frame->width - 1) / state->tile_edge) {
            prefetch_end_x = ((int)frame->width - 1) / state->tile_edge;
        }
        if (prefetch_end_y > ((int)frame->height - 1) / state->tile_edge) {
            prefetch_end_y = ((int)frame->height - 1) / state->tile_edge;
        }
        for (int tile_y_index = prefetch_start_y; tile_y_index <= prefetch_end_y; ++tile_y_index) {
            for (int tile_x_index = prefetch_start_x; tile_x_index <= prefetch_end_x; ++tile_x_index) {
                CoreResult touch_r;
                if (tile_x_index >= start_tile_x && tile_x_index <= end_tile_x &&
                    tile_y_index >= start_tile_y && tile_y_index <= end_tile_y) {
                    continue;
                }
                touch_r = datalab_raster_tile_cache_touch(state,
                                                          frame,
                                                          tile_x_index,
                                                          tile_y_index,
                                                          NULL);
                if (touch_r.code != CORE_OK) {
                    return touch_r;
                }
            }
        }
    }
    return core_result_ok();
}

CoreResult datalab_raster_texture_state_init(SDL_Renderer *renderer,
                                             uint32_t content_width,
                                             uint32_t content_height,
                                             DatalabRasterTextureState *state) {
    if (!renderer || !state || content_width == 0u || content_height == 0u) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid raster texture state init request" };
    }

    datalab_raster_texture_state_reset(state);
    datalab_raster_texture_state_note_renderer_limits(renderer, state);
    if (!datalab_raster_texture_state_needs_tiling(state, content_width, content_height)) {
        state->full_texture = SDL_CreateTexture(renderer,
                                                SDL_PIXELFORMAT_RGBA32,
                                                SDL_TEXTUREACCESS_STREAMING,
                                                (int)content_width,
                                                (int)content_height);
        if (state->full_texture) {
            state->use_tiled = 0;
            return core_result_ok();
        }
    }

    state->use_tiled = 1;
    state->tile_edge = datalab_raster_tile_clamp_edge(state->max_texture_width, state->max_texture_height);
    state->prefetch_radius = DATALAB_RASTER_TILE_PREFETCH_RADIUS_DEFAULT;
    state->cache_capacity = datalab_raster_tile_cache_target_capacity(renderer, state->tile_edge);
    state->cache_entries = (DatalabRasterTileCacheEntry *)core_alloc((size_t)state->cache_capacity *
                                                                     sizeof(DatalabRasterTileCacheEntry));
    if (state->cache_entries) {
        memset(state->cache_entries, 0, (size_t)state->cache_capacity * sizeof(DatalabRasterTileCacheEntry));
        for (int i = 0; i < state->cache_capacity; ++i) {
            state->cache_entries[i].texture = SDL_CreateTexture(renderer,
                                                                SDL_PIXELFORMAT_RGBA32,
                                                                SDL_TEXTUREACCESS_STREAMING,
                                                                state->tile_edge,
                                                                state->tile_edge);
            if (!state->cache_entries[i].texture) {
                datalab_raster_texture_state_destroy_cache(state);
                break;
            }
            datalab_raster_tile_cache_reset_entry(&state->cache_entries[i]);
        }
    }
    if (!state->cache_entries) {
        state->tile_texture = SDL_CreateTexture(renderer,
                                                SDL_PIXELFORMAT_RGBA32,
                                                SDL_TEXTUREACCESS_STREAMING,
                                                state->tile_edge,
                                                state->tile_edge);
        if (!state->tile_texture) {
            datalab_raster_texture_state_destroy(state);
            return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
        }
    }
    return core_result_ok();
}

void datalab_raster_texture_state_destroy(DatalabRasterTextureState *state) {
    if (!state) {
        return;
    }
    if (state->full_texture) {
        SDL_DestroyTexture(state->full_texture);
    }
    datalab_raster_texture_state_destroy_cache(state);
    if (state->tile_texture) {
        SDL_DestroyTexture(state->tile_texture);
    }
    datalab_raster_texture_state_reset(state);
}

CoreResult datalab_raster_render_frame(SDL_Renderer *renderer,
                                       const DatalabFrame *frame,
                                       const DatalabSketchRenderDeriveFrame *derive,
                                       DatalabRasterTextureState *state) {
    if (!renderer || !frame || !derive || !state || !frame->drawing_rgba) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid raster render frame request" };
    }
    if (!state->use_tiled) {
        if (!state->full_texture) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "missing full raster texture" };
        }
        if (SDL_UpdateTexture(state->full_texture, NULL, frame->drawing_rgba, (int)frame->width * 4) != 0) {
            return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
        }
        if (SDL_RenderCopy(renderer, state->full_texture, NULL, &derive->dst) != 0) {
            return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
        }
        return core_result_ok();
    }
    return datalab_raster_render_tiled(renderer, frame, derive, state);
}
