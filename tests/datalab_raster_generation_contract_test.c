#include <stdio.h>
#include <string.h>

#include "render/render_view_internal.h"

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "raster-generation-contract: %s\n", message);
    }
    return condition;
}

int main(void) {
    DatalabRasterTextureState state;
    DatalabRasterTileCacheEntry entry;

    memset(&state, 0, sizeof(state));
    datalab_raster_texture_state_note_content_generation(&state, 41u);
    datalab_raster_texture_state_note_resource_recreation(&state);
    if (!require(datalab_raster_texture_state_upload_required(&state),
                 "initial content and resource generations must require upload")) return 1;
    datalab_raster_texture_state_note_upload(&state, 64u);
    if (!require(!datalab_raster_texture_state_upload_required(&state),
                 "heartbeat, pan, HUD, and invalidation presentation reuse must not upload")) return 1;

    datalab_raster_texture_state_note_content_generation(&state, 42u);
    if (!require(datalab_raster_texture_state_upload_required(&state),
                 "content replacement must require exactly one upload")) return 1;
    datalab_raster_texture_state_note_upload(&state, 64u);
    datalab_raster_texture_state_note_resource_recreation(&state);
    if (!require(datalab_raster_texture_state_upload_required(&state),
                 "resize or renderer-resource recreation must require reupload")) return 1;
    datalab_raster_texture_state_note_upload(&state, 64u);
    if (!require(state.upload_count == 3u && state.upload_byte_count == 192u,
                 "instrumentation must count only initial, content, and resource uploads")) return 1;

    memset(&entry, 0, sizeof(entry));
    entry.valid = 1;
    entry.tile_x_index = 2;
    entry.tile_y_index = 3;
    entry.content_generation = state.content_generation;
    entry.resource_generation = state.resource_generation;
    if (!require(datalab_raster_tile_cache_entry_matches(&entry, &state, 2, 3),
                 "tiled residency must reuse a matching content/resource generation")) return 1;
    datalab_raster_texture_state_note_resource_recreation(&state);
    if (!require(!datalab_raster_tile_cache_entry_matches(&entry, &state, 2, 3),
                 "tiled residency must reject stale renderer resources")) return 1;

    if (!require(datalab_raster_sampling_scale_mode(DATALAB_SAMPLING_MODE_NEAREST) == SDL_ScaleModeNearest &&
                 datalab_raster_sampling_scale_mode(DATALAB_SAMPLING_MODE_LINEAR) == SDL_ScaleModeLinear &&
                 datalab_raster_sampling_scale_mode(DATALAB_SAMPLING_MODE_DEFAULT) == SDL_ScaleModeNearest,
                 "persisted sampling modes must map explicitly to SDL texture scale modes")) return 1;

    puts("datalab raster generation contract test passed");
    return 0;
}
