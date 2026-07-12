#include "data/pack_loader.h"

#include <stdio.h>
#include <string.h>

#include "core_pack.h"
#include "data/pack_loader_internal.h"
#include "data/pack_loader_sketch.h"

enum {
    DATALAB_PACK_MAX_PHYSICS_CELLS = 67108864u,
    DATALAB_PACK_MAX_DAW_POINTS = 16777216u,
    DATALAB_PACK_MAX_DAW_MARKERS = 1048576u,
    DATALAB_PACK_MAX_TRACE_SAMPLES = 4194304u,
    DATALAB_PACK_MAX_TRACE_MARKERS = 1048576u,
    DATALAB_PACK_MAX_MANIFEST_BYTES = 8388608u
};

typedef struct Vf2dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float cell_size;
    uint32_t obstacle_mask_crc32;
} Vf2dHeaderCanonical;

typedef struct DawHeaderCanonical {
    uint32_t version;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t samples_per_pixel;
    uint64_t point_count;
    uint64_t start_frame;
    uint64_t end_frame;
    uint64_t project_duration_frames;
} DawHeaderCanonical;

typedef struct TraceHeaderCanonical {
    uint32_t trace_profile_version;
    uint32_t reserved;
    uint64_t sample_count;
    uint64_t marker_count;
} TraceHeaderCanonical;

typedef struct Vf3dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float origin_z;
    float voxel_size;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    uint32_t solid_mask_crc32;
} Vf3dHeaderCanonical;

typedef struct GrowthHeaderCanonical {
    char schema_id[64];
    uint32_t schema_version;
    uint32_t width;
    uint32_t height;
    uint32_t cell_count;
    uint32_t field_count;
    uint32_t steps_executed;
    float fixed_dt_seconds;
    uint32_t reserved[8];
} GrowthHeaderCanonical;

typedef struct LineDrawingHeaderCanonical { uint32_t schema_version, anchor_count, wall_count, curved_anchor_count, persistent_anchor_count; } LineDrawingHeaderCanonical;
typedef struct LineDrawingAnchorPackBaseV1 {
    float x, y, handle_in_length, handle_in_angle_deg, handle_out_length, handle_out_angle_deg;
    uint32_t persistent, anchor_type;
} LineDrawingAnchorPackBaseV1;
typedef struct LineDrawingWallRowV1 { uint32_t a, b, lock_length; } LineDrawingWallRowV1;

static int datalab_u64_fits_size(uint64_t value) {
    return value <= (uint64_t)SIZE_MAX;
}

static CoreResult datalab_checked_float_byte_count(size_t value_count, size_t *out_bytes) {
    if (!out_bytes) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_bytes = 0u;
    if (value_count > SIZE_MAX / sizeof(float)) {
        return (CoreResult){ CORE_ERR_FORMAT, "float field allocation too large" };
    }
    *out_bytes = value_count * sizeof(float);
    return core_result_ok();
}

void datalab_frame_init(DatalabFrame *frame) {
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
    frame->profile = DATALAB_PROFILE_UNKNOWN;
}

void datalab_frame_free(DatalabFrame *frame) {
    if (!frame) return;
    core_free(frame->density);
    core_free(frame->velx);
    core_free(frame->vely);
    core_free(frame->wave_min);
    core_free(frame->wave_max);
    core_free(frame->markers);
    core_free(frame->trace_samples);
    core_free(frame->trace_markers);
    core_free(frame->drawing_rgba);
    core_free(frame->line_anchors);
    core_free(frame->line_walls);
    core_free(frame->manifest_json);
    memset(frame, 0, sizeof(*frame));
}

static CoreResult read_float_chunk(CorePackReader *reader,
                                   const CorePackChunkInfo *chunk,
                                   float **out_values,
                                   size_t expected_count) {
    if (!reader || !chunk || !out_values) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    size_t expected_bytes = 0u;
    CoreResult size_r = datalab_checked_float_byte_count(expected_count, &expected_bytes);
    if (size_r.code != CORE_OK) {
        return size_r;
    }
    if (chunk->size != (uint64_t)expected_bytes) {
        CoreResult r = { CORE_ERR_FORMAT, "unexpected field chunk size" };
        return r;
    }

    float *values = (float *)core_alloc(expected_bytes);
    if (!values) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    CoreResult rr = core_pack_reader_read_chunk_data(reader, chunk, values, chunk->size);
    if (rr.code != CORE_OK) {
        core_free(values);
        return rr;
    }

    *out_values = values;
    return core_result_ok();
}

static CoreResult read_marker_chunk(CorePackReader *reader,
                                    const CorePackChunkInfo *chunk,
                                    DatalabDawMarker **out_markers,
                                    size_t *out_count) {
    if (!reader || !chunk || !out_markers || !out_count) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (chunk->size % sizeof(DatalabDawMarker) != 0) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid marker chunk size" };
        return r;
    }
    if (!datalab_u64_fits_size(chunk->size)) {
        CoreResult r = { CORE_ERR_FORMAT, "marker chunk too large" };
        return r;
    }

    size_t count = (size_t)(chunk->size / sizeof(DatalabDawMarker));
    if (count > DATALAB_PACK_MAX_DAW_MARKERS) {
        CoreResult r = { CORE_ERR_FORMAT, "marker count exceeds safety limit" };
        return r;
    }
    if (count == 0) {
        *out_markers = NULL;
        *out_count = 0;
        return core_result_ok();
    }

    DatalabDawMarker *markers = (DatalabDawMarker *)core_alloc((size_t)chunk->size);
    if (!markers) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    CoreResult rr = core_pack_reader_read_chunk_data(reader, chunk, markers, chunk->size);
    if (rr.code != CORE_OK) {
        core_free(markers);
        return rr;
    }

    *out_markers = markers;
    *out_count = count;
    return core_result_ok();
}

CoreResult datalab_pack_loader_read_optional_manifest(CorePackReader *reader, DatalabFrame *out_frame) {
    CorePackChunkInfo json;
    CoreResult fj = core_pack_reader_find_chunk(reader, "JSON", 0, &json);
    if (fj.code != CORE_OK || json.size == 0) {
        return core_result_ok();
    }
    if (json.size > DATALAB_PACK_MAX_MANIFEST_BYTES || json.size > (uint64_t)(SIZE_MAX - 1u)) {
        CoreResult r = { CORE_ERR_FORMAT, "manifest chunk exceeds safety limit" };
        return r;
    }

    char *manifest = (char *)core_alloc((size_t)json.size + 1u);
    if (!manifest) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    CoreResult rj = core_pack_reader_read_chunk_data(reader, &json, manifest, json.size);
    if (rj.code != CORE_OK) {
        core_free(manifest);
        return rj;
    }

    manifest[json.size] = '\0';
    out_frame->manifest_json = manifest;
    out_frame->manifest_size = (size_t)json.size;
    return core_result_ok();
}

static CoreResult load_physics_profile(CorePackReader *reader, const CorePackChunkInfo *vfhd, DatalabFrame *out_frame) {
    if (!reader || !vfhd || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (vfhd->size < sizeof(Vf2dHeaderCanonical)) {
        CoreResult r = { CORE_ERR_FORMAT, "vfhd chunk too small" };
        return r;
    }

    Vf2dHeaderCanonical hdr;
    memset(&hdr, 0, sizeof(hdr));
    CoreResult read_hdr = core_pack_reader_read_chunk_data(reader, vfhd, &hdr, sizeof(hdr));
    if (read_hdr.code != CORE_OK) {
        return read_hdr;
    }

    out_frame->profile = DATALAB_PROFILE_PHYSICS;
    out_frame->vf_version = hdr.version;
    out_frame->width = hdr.grid_w;
    out_frame->height = hdr.grid_h;
    out_frame->time_seconds = hdr.time_seconds;
    out_frame->frame_index = hdr.frame_index;
    out_frame->dt_seconds = hdr.dt_seconds;
    out_frame->origin_x = hdr.origin_x;
    out_frame->origin_y = hdr.origin_y;
    out_frame->cell_size = hdr.cell_size;
    out_frame->obstacle_mask_crc32 = hdr.obstacle_mask_crc32;

    uint64_t cell_count64 = (uint64_t)out_frame->width * (uint64_t)out_frame->height;
    size_t count = 0u;
    if (out_frame->width == 0u || out_frame->height == 0u ||
        cell_count64 > DATALAB_PACK_MAX_PHYSICS_CELLS ||
        !datalab_u64_fits_size(cell_count64)) {
        CoreResult r = { CORE_ERR_FORMAT, "physics grid bounds exceed safety limit" };
        return r;
    }
    count = (size_t)cell_count64;

    CorePackChunkInfo dens;
    CorePackChunkInfo velx;
    CorePackChunkInfo vely;

    CoreResult fd = core_pack_reader_find_chunk(reader, "DENS", 0, &dens);
    CoreResult fx = core_pack_reader_find_chunk(reader, "VELX", 0, &velx);
    CoreResult fy = core_pack_reader_find_chunk(reader, "VELY", 0, &vely);
    if (fd.code != CORE_OK || fx.code != CORE_OK || fy.code != CORE_OK) {
        CoreResult r = { CORE_ERR_NOT_FOUND, "required field chunks missing" };
        return r;
    }

    CoreResult rd = read_float_chunk(reader, &dens, &out_frame->density, count);
    if (rd.code != CORE_OK) {
        return rd;
    }

    CoreResult rx = read_float_chunk(reader, &velx, &out_frame->velx, count);
    if (rx.code != CORE_OK) {
        return rx;
    }

    CoreResult ry = read_float_chunk(reader, &vely, &out_frame->vely, count);
    if (ry.code != CORE_OK) {
        return ry;
    }

    return datalab_pack_loader_read_optional_manifest(reader, out_frame);
}

static CoreResult load_volume_profile(CorePackReader *reader,
                                      const CorePackChunkInfo *vf3h,
                                      DatalabFrame *out_frame) {
    Vf3dHeaderCanonical hdr = {0};
    CorePackChunkInfo dens = {{0}, 0u, 0u};
    CorePackChunkInfo velx = {{0}, 0u, 0u};
    CorePackChunkInfo vely = {{0}, 0u, 0u};
    uint64_t plane_count = 0u;
    uint64_t volume_count = 0u;
    uint64_t plane_bytes = 0u;
    uint64_t volume_bytes = 0u;
    uint64_t slice_offset = 0u;
    size_t plane_bytes_size = 0u;
    CoreResult result;
    if (!reader || !vf3h || !out_frame || vf3h->size < sizeof(hdr)) {
        return (CoreResult){ CORE_ERR_FORMAT, "vf3h chunk too small" };
    }
    result = core_pack_reader_read_chunk_data(reader, vf3h, &hdr, sizeof(hdr));
    if (result.code != CORE_OK) {
        return result;
    }
    plane_count = (uint64_t)hdr.grid_w * (uint64_t)hdr.grid_h;
    if (hdr.grid_d == 0u || plane_count > UINT64_MAX / (uint64_t)hdr.grid_d) {
        return (CoreResult){ CORE_ERR_FORMAT, "vf3h volume bounds exceed safety limit" };
    }
    volume_count = plane_count * (uint64_t)hdr.grid_d;
    if (hdr.version == 0u || hdr.grid_w == 0u || hdr.grid_h == 0u || hdr.grid_d == 0u ||
        plane_count > DATALAB_PACK_MAX_PHYSICS_CELLS ||
        volume_count > DATALAB_PACK_MAX_PHYSICS_CELLS ||
        !datalab_u64_fits_size(plane_count) ||
        !datalab_u64_fits_size(volume_count) ||
        plane_count > UINT64_MAX / sizeof(float) ||
        volume_count > UINT64_MAX / sizeof(float)) {
        return (CoreResult){ CORE_ERR_FORMAT, "vf3h volume bounds exceed safety limit" };
    }
    plane_bytes = plane_count * sizeof(float);
    volume_bytes = volume_count * sizeof(float);
    plane_bytes_size = (size_t)plane_bytes;
    result = core_pack_reader_find_chunk(reader, "DENS", 0u, &dens);
    if (result.code == CORE_OK) result = core_pack_reader_find_chunk(reader, "VELX", 0u, &velx);
    if (result.code == CORE_OK) result = core_pack_reader_find_chunk(reader, "VELY", 0u, &vely);
    if (result.code != CORE_OK || dens.size != volume_bytes || velx.size != volume_bytes || vely.size != volume_bytes) {
        return (CoreResult){ CORE_ERR_FORMAT, "vf3h field chunks are missing or invalid" };
    }
    out_frame->volume_slice_index = hdr.grid_d / 2u;
    slice_offset = (uint64_t)out_frame->volume_slice_index * plane_bytes;
    out_frame->density = (float *)core_alloc(plane_bytes_size);
    out_frame->velx = (float *)core_alloc(plane_bytes_size);
    out_frame->vely = (float *)core_alloc(plane_bytes_size);
    if (!out_frame->density || !out_frame->velx || !out_frame->vely) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    result = core_pack_reader_read_chunk_slice(reader, &dens, slice_offset, out_frame->density, plane_bytes);
    if (result.code == CORE_OK) result = core_pack_reader_read_chunk_slice(reader, &velx, slice_offset, out_frame->velx, plane_bytes);
    if (result.code == CORE_OK) result = core_pack_reader_read_chunk_slice(reader, &vely, slice_offset, out_frame->vely, plane_bytes);
    if (result.code != CORE_OK) {
        return result;
    }
    out_frame->profile = DATALAB_PROFILE_VOLUME;
    out_frame->vf_version = hdr.version;
    out_frame->width = hdr.grid_w;
    out_frame->height = hdr.grid_h;
    out_frame->volume_depth = hdr.grid_d;
    out_frame->time_seconds = hdr.time_seconds;
    out_frame->frame_index = hdr.frame_index;
    out_frame->dt_seconds = hdr.dt_seconds;
    out_frame->origin_x = hdr.origin_x;
    out_frame->origin_y = hdr.origin_y;
    out_frame->origin_z = hdr.origin_z;
    out_frame->cell_size = hdr.voxel_size;
    out_frame->voxel_size = hdr.voxel_size;
    out_frame->obstacle_mask_crc32 = hdr.solid_mask_crc32;
    return datalab_pack_loader_read_optional_manifest(reader, out_frame);
}

static CoreResult load_growth_profile(CorePackReader *reader, const CorePackChunkInfo *gfhd, DatalabFrame *out_frame) {
    GrowthHeaderCanonical hdr = {0};
    CorePackChunkInfo field = {{0}, 0u, 0u};
    uint64_t count = 0u;
    size_t bytes = 0u;
    CoreResult result;
    if (!reader || !gfhd || !out_frame || gfhd->size < sizeof(hdr)) return (CoreResult){ CORE_ERR_FORMAT, "gfhd chunk too small" };
    result = core_pack_reader_read_chunk_data(reader, gfhd, &hdr, sizeof(hdr));
    if (result.code != CORE_OK) return result;
    count = (uint64_t)hdr.width * (uint64_t)hdr.height;
    if (hdr.schema_version == 0u || hdr.width == 0u || hdr.height == 0u || hdr.cell_count != count ||
        count > DATALAB_PACK_MAX_PHYSICS_CELLS || !datalab_u64_fits_size(count) ||
        datalab_checked_float_byte_count((size_t)count, &bytes).code != CORE_OK) return (CoreResult){ CORE_ERR_FORMAT, "gfhd field bounds invalid" };
    result = core_pack_reader_find_chunk(reader, "OCCP", 0u, &field);
    if (result.code == CORE_OK) {
        snprintf(out_frame->growth_primary_field, sizeof(out_frame->growth_primary_field), "%s", "occupancy");
    } else {
        result = core_pack_reader_find_chunk(reader, "FAMT", 0u, &field);
        if (result.code == CORE_OK) {
            snprintf(out_frame->growth_primary_field, sizeof(out_frame->growth_primary_field), "%s", "fuel_amount");
        }
    }
    if (result.code != CORE_OK || field.size != bytes) return (CoreResult){ CORE_ERR_FORMAT, "gfhd primary field is missing or invalid" };
    out_frame->density = (float *)core_alloc(bytes);
    out_frame->velx = (float *)core_alloc(bytes);
    out_frame->vely = (float *)core_alloc(bytes);
    if (!out_frame->density || !out_frame->velx || !out_frame->vely) return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    result = core_pack_reader_read_chunk_data(reader, &field, out_frame->density, field.size);
    if (result.code != CORE_OK) return result;
    memset(out_frame->velx, 0, bytes);
    memset(out_frame->vely, 0, bytes);
    out_frame->profile = DATALAB_PROFILE_GROWTH;
    out_frame->width = hdr.width; out_frame->height = hdr.height; out_frame->growth_steps_executed = hdr.steps_executed;
    out_frame->dt_seconds = hdr.fixed_dt_seconds;
    snprintf(out_frame->growth_schema_id, sizeof(out_frame->growth_schema_id), "%s", hdr.schema_id);
    return datalab_pack_loader_read_optional_manifest(reader, out_frame);
}

static CoreResult load_line_diagnostic_profile(CorePackReader *reader, const CorePackChunkInfo *ldhd, DatalabFrame *out_frame) {
    LineDrawingHeaderCanonical hdr = {0};
    CorePackChunkInfo anchors = {{0}, 0u, 0u};
    CorePackChunkInfo walls = {{0}, 0u, 0u};
    uint64_t anchor_bytes = 0u;
    uint64_t wall_bytes = 0u;
    CoreResult result;
    if (!reader || !ldhd || !out_frame || ldhd->size < sizeof(hdr)) return (CoreResult){ CORE_ERR_FORMAT, "ldhd chunk too small" };
    result = core_pack_reader_read_chunk_data(reader, ldhd, &hdr, sizeof(hdr));
    if (result.code != CORE_OK) return result;
    if (hdr.schema_version == 0u || hdr.anchor_count > DATALAB_PACK_MAX_PHYSICS_CELLS || hdr.wall_count > DATALAB_PACK_MAX_PHYSICS_CELLS ||
        (uint64_t)hdr.anchor_count > UINT64_MAX / sizeof(LineDrawingAnchorPackBaseV1) ||
        (uint64_t)hdr.wall_count > UINT64_MAX / sizeof(LineDrawingWallRowV1)) {
        return (CoreResult){ CORE_ERR_FORMAT, "ldhd diagnostic bounds invalid" };
    }
    anchor_bytes = (uint64_t)hdr.anchor_count * sizeof(LineDrawingAnchorPackBaseV1);
    wall_bytes = (uint64_t)hdr.wall_count * sizeof(LineDrawingWallRowV1);
    result = core_pack_reader_find_chunk(reader, "LDAN", 0u, &anchors);
    if (result.code == CORE_OK) result = core_pack_reader_find_chunk(reader, "LDWL", 0u, &walls);
    if (result.code == CORE_OK && (anchors.size != anchor_bytes || walls.size != wall_bytes)) {
        return (CoreResult){ CORE_ERR_FORMAT, "line diagnostic geometry chunk sizes invalid" };
    }
    if (result.code == CORE_OK && hdr.anchor_count > 0u) {
        LineDrawingAnchorPackBaseV1 *source = (LineDrawingAnchorPackBaseV1 *)core_alloc((size_t)anchor_bytes);
        out_frame->line_anchors = (DatalabLineAnchor *)core_alloc((size_t)hdr.anchor_count * sizeof(*out_frame->line_anchors));
        if (!source || !out_frame->line_anchors) { core_free(source); return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" }; }
        result = core_pack_reader_read_chunk_data(reader, &anchors, source, anchors.size);
        if (result.code == CORE_OK) for (uint32_t i = 0u; i < hdr.anchor_count; ++i) {
            out_frame->line_anchors[i] = (DatalabLineAnchor){ source[i].x, source[i].y, source[i].persistent, source[i].anchor_type };
        }
        core_free(source);
        if (result.code != CORE_OK) return result;
    }
    if (result.code == CORE_OK && hdr.wall_count > 0u) {
        LineDrawingWallRowV1 *source = (LineDrawingWallRowV1 *)core_alloc((size_t)wall_bytes);
        out_frame->line_walls = (DatalabLineWall *)core_alloc((size_t)hdr.wall_count * sizeof(*out_frame->line_walls));
        if (!source || !out_frame->line_walls) { core_free(source); return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" }; }
        result = core_pack_reader_read_chunk_data(reader, &walls, source, walls.size);
        if (result.code == CORE_OK) for (uint32_t i = 0u; i < hdr.wall_count; ++i) {
            if (source[i].a >= hdr.anchor_count || source[i].b >= hdr.anchor_count) { core_free(source); return (CoreResult){ CORE_ERR_FORMAT, "line wall anchor index invalid" }; }
            out_frame->line_walls[i] = (DatalabLineWall){ source[i].a, source[i].b, source[i].lock_length };
        }
        core_free(source);
        if (result.code != CORE_OK) return result;
    }
    out_frame->profile = DATALAB_PROFILE_LINE_DIAGNOSTIC;
    out_frame->line_schema_version = hdr.schema_version;
    out_frame->line_anchor_count = hdr.anchor_count;
    out_frame->line_wall_count = hdr.wall_count;
    out_frame->line_curved_anchor_count = hdr.curved_anchor_count;
    out_frame->line_persistent_anchor_count = hdr.persistent_anchor_count;
    return datalab_pack_loader_read_optional_manifest(reader, out_frame);
}

static CoreResult load_daw_profile(CorePackReader *reader, const CorePackChunkInfo *dawh, DatalabFrame *out_frame) {
    if (!reader || !dawh || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (dawh->size < sizeof(DawHeaderCanonical)) {
        CoreResult r = { CORE_ERR_FORMAT, "dawh chunk too small" };
        return r;
    }

    DawHeaderCanonical hdr;
    memset(&hdr, 0, sizeof(hdr));
    CoreResult read_hdr = core_pack_reader_read_chunk_data(reader, dawh, &hdr, sizeof(hdr));
    if (read_hdr.code != CORE_OK) {
        return read_hdr;
    }

    out_frame->profile = DATALAB_PROFILE_DAW;
    out_frame->daw_version = hdr.version;
    out_frame->sample_rate = hdr.sample_rate;
    out_frame->channels = hdr.channels;
    out_frame->samples_per_pixel = hdr.samples_per_pixel;
    out_frame->point_count = hdr.point_count;
    out_frame->start_frame = hdr.start_frame;
    out_frame->end_frame = hdr.end_frame;
    out_frame->project_duration_frames = hdr.project_duration_frames;

    if (hdr.point_count == 0 || hdr.point_count > DATALAB_PACK_MAX_DAW_POINTS ||
        !datalab_u64_fits_size(hdr.point_count)) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid daw point count" };
        return r;
    }

    CorePackChunkInfo wmin;
    CorePackChunkInfo wmax;
    CorePackChunkInfo mrks;

    CoreResult fmin = core_pack_reader_find_chunk(reader, "WMIN", 0, &wmin);
    CoreResult fmax = core_pack_reader_find_chunk(reader, "WMAX", 0, &wmax);
    CoreResult fmrk = core_pack_reader_find_chunk(reader, "MRKS", 0, &mrks);
    if (fmin.code != CORE_OK || fmax.code != CORE_OK || fmrk.code != CORE_OK) {
        CoreResult r = { CORE_ERR_NOT_FOUND, "required daw chunks missing" };
        return r;
    }

    CoreResult rmin = read_float_chunk(reader, &wmin, &out_frame->wave_min, (size_t)hdr.point_count);
    if (rmin.code != CORE_OK) {
        return rmin;
    }

    CoreResult rmax = read_float_chunk(reader, &wmax, &out_frame->wave_max, (size_t)hdr.point_count);
    if (rmax.code != CORE_OK) {
        return rmax;
    }

    CoreResult rmk = read_marker_chunk(reader, &mrks, &out_frame->markers, &out_frame->marker_count);
    if (rmk.code != CORE_OK) {
        return rmk;
    }

    return datalab_pack_loader_read_optional_manifest(reader, out_frame);
}

static CoreResult load_trace_profile(CorePackReader *reader, const CorePackChunkInfo *trhd, DatalabFrame *out_frame) {
    TraceHeaderCanonical hdr;
    CorePackChunkInfo trsm;
    CorePackChunkInfo trev;
    CoreResult rr;

    if (!reader || !trhd || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (trhd->size < sizeof(TraceHeaderCanonical)) {
        CoreResult r = { CORE_ERR_FORMAT, "trhd chunk too small" };
        return r;
    }

    memset(&hdr, 0, sizeof(hdr));
    rr = core_pack_reader_read_chunk_data(reader, trhd, &hdr, sizeof(hdr));
    if (rr.code != CORE_OK) {
        return rr;
    }

    out_frame->profile = DATALAB_PROFILE_TRACE;
    out_frame->trace_version = hdr.trace_profile_version;
    if (hdr.sample_count > DATALAB_PACK_MAX_TRACE_SAMPLES ||
        hdr.marker_count > DATALAB_PACK_MAX_TRACE_MARKERS ||
        !datalab_u64_fits_size(hdr.sample_count) ||
        !datalab_u64_fits_size(hdr.marker_count)) {
        CoreResult r = { CORE_ERR_FORMAT, "trace counts exceed safety limit" };
        return r;
    }

    out_frame->trace_sample_count = (size_t)hdr.sample_count;
    out_frame->trace_marker_count = (size_t)hdr.marker_count;

    if (hdr.sample_count > 0u) {
        rr = core_pack_reader_find_chunk(reader, "TRSM", 0, &trsm);
        if (rr.code != CORE_OK) {
            CoreResult r = { CORE_ERR_NOT_FOUND, "required trace samples chunk missing" };
            return r;
        }
        if (hdr.sample_count > UINT64_MAX / sizeof(DatalabTraceSample) ||
            trsm.size != hdr.sample_count * sizeof(DatalabTraceSample)) {
            CoreResult r = { CORE_ERR_FORMAT, "invalid trsm chunk size" };
            return r;
        }
        out_frame->trace_samples = (DatalabTraceSample *)core_alloc((size_t)trsm.size);
        if (!out_frame->trace_samples) {
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
        rr = core_pack_reader_read_chunk_data(reader, &trsm, out_frame->trace_samples, trsm.size);
        if (rr.code != CORE_OK) {
            return rr;
        }
    }

    if (hdr.marker_count > 0u) {
        rr = core_pack_reader_find_chunk(reader, "TREV", 0, &trev);
        if (rr.code != CORE_OK) {
            CoreResult r = { CORE_ERR_NOT_FOUND, "required trace marker chunk missing" };
            return r;
        }
        if (hdr.marker_count > UINT64_MAX / sizeof(DatalabTraceMarker) ||
            trev.size != hdr.marker_count * sizeof(DatalabTraceMarker)) {
            CoreResult r = { CORE_ERR_FORMAT, "invalid trev chunk size" };
            return r;
        }
        out_frame->trace_markers = (DatalabTraceMarker *)core_alloc((size_t)trev.size);
        if (!out_frame->trace_markers) {
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
        rr = core_pack_reader_read_chunk_data(reader, &trev, out_frame->trace_markers, trev.size);
        if (rr.code != CORE_OK) {
            return rr;
        }
    }

    return datalab_pack_loader_read_optional_manifest(reader, out_frame);
}

CoreResult datalab_load_pack(const char *pack_path, DatalabFrame *out_frame) {
    if (!pack_path || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    datalab_frame_init(out_frame);

    CorePackReader reader;
    CoreResult open_r = core_pack_reader_open(pack_path, &reader);
    if (open_r.code != CORE_OK) {
        return open_r;
    }

    out_frame->chunk_count = core_pack_reader_chunk_count(&reader);

    CorePackChunkInfo vfhd;
    CorePackChunkInfo vf3h;
    CorePackChunkInfo gfhd;
    CorePackChunkInfo ldhd;
    CorePackChunkInfo dawh;
    CorePackChunkInfo trhd;
    CorePackChunkInfo dps3;
    CorePackChunkInfo dps2;
    CoreResult find_vfhd = core_pack_reader_find_chunk(&reader, "VFHD", 0, &vfhd);
    CoreResult find_vf3h = core_pack_reader_find_chunk(&reader, "VF3H", 0, &vf3h);
    CoreResult find_gfhd = core_pack_reader_find_chunk(&reader, "GFHD", 0, &gfhd);
    CoreResult find_ldhd = core_pack_reader_find_chunk(&reader, "LDHD", 0, &ldhd);
    CoreResult find_dawh = core_pack_reader_find_chunk(&reader, "DAWH", 0, &dawh);
    CoreResult find_trhd = core_pack_reader_find_chunk(&reader, "TRHD", 0, &trhd);
    CoreResult find_dps3 = core_pack_reader_find_chunk(&reader, "DPS3", 0, &dps3);
    CoreResult find_dps2 = core_pack_reader_find_chunk(&reader, "DPS2", 0, &dps2);

    CoreResult load_r;
    if (find_trhd.code == CORE_OK) {
        load_r = load_trace_profile(&reader, &trhd, out_frame);
    } else if (find_vf3h.code == CORE_OK) {
        load_r = load_volume_profile(&reader, &vf3h, out_frame);
    } else if (find_gfhd.code == CORE_OK) {
        load_r = load_growth_profile(&reader, &gfhd, out_frame);
    } else if (find_ldhd.code == CORE_OK) {
        load_r = load_line_diagnostic_profile(&reader, &ldhd, out_frame);
    } else if (find_vfhd.code == CORE_OK) {
        load_r = load_physics_profile(&reader, &vfhd, out_frame);
    } else if (find_dawh.code == CORE_OK) {
        load_r = load_daw_profile(&reader, &dawh, out_frame);
    } else if (find_dps3.code == CORE_OK || find_dps2.code == CORE_OK) {
        load_r = datalab_pack_loader_load_sketch_profile(&reader,
                                     find_dps3.code == CORE_OK ? &dps3 : NULL,
                                     find_dps2.code == CORE_OK ? &dps2 : NULL,
                                     out_frame);
    } else {
        load_r.code = CORE_ERR_NOT_FOUND;
        load_r.message = "unsupported pack profile (missing TRHD/VF3H/VFHD/DAWH/DPS3/DPS2)";
    }

    if (load_r.code != CORE_OK) {
        datalab_frame_free(out_frame);
        core_pack_reader_close(&reader);
        return load_r;
    }

    core_pack_reader_close(&reader);
    return core_result_ok();
}

void datalab_print_frame_summary(const char *pack_path, const DatalabFrame *frame) {
    if (!frame) {
        return;
    }

    printf("pack=%s\n", pack_path ? pack_path : "<unknown>");
    printf("  profile=%s chunks=%zu\n",
           frame->profile == DATALAB_PROFILE_PHYSICS ? "physics" :
           frame->profile == DATALAB_PROFILE_DAW ? "daw" :
           frame->profile == DATALAB_PROFILE_TRACE ? "trace" :
           frame->profile == DATALAB_PROFILE_SKETCH ? "sketch" :
           frame->profile == DATALAB_PROFILE_VOLUME ? "volume" : frame->profile == DATALAB_PROFILE_GROWTH ? "growth" : frame->profile == DATALAB_PROFILE_LINE_DIAGNOSTIC ? "line_diagnostic" : "unknown",
           frame->chunk_count);

    if (frame->profile == DATALAB_PROFILE_PHYSICS || frame->profile == DATALAB_PROFILE_VOLUME) {
        printf("  grid=%ux%u frame=%llu time=%.6f dt=%.6f\n",
               frame->width,
               frame->height,
               (unsigned long long)frame->frame_index,
               frame->time_seconds,
               frame->dt_seconds);
        printf("  origin=(%.3f,%.3f) cell=%.3f crc=%u\n",
               frame->origin_x,
               frame->origin_y,
               frame->cell_size,
               frame->obstacle_mask_crc32);
        if (frame->profile == DATALAB_PROFILE_VOLUME) {
            printf("  volume=xy-slice z=%u/%u origin_z=%.3f voxel=%.3f\n",
                   frame->volume_slice_index,
                   frame->volume_depth,
                   frame->origin_z,
                   frame->voxel_size);
        }
    } else if (frame->profile == DATALAB_PROFILE_DAW) {
        printf("  daw_version=%u sample_rate=%u channels=%u samples_per_pixel=%u\n",
               frame->daw_version,
               frame->sample_rate,
               frame->channels,
               frame->samples_per_pixel);
        printf("  points=%llu markers=%zu start=%llu end=%llu project_end=%llu\n",
               (unsigned long long)frame->point_count,
               frame->marker_count,
               (unsigned long long)frame->start_frame,
               (unsigned long long)frame->end_frame,
               (unsigned long long)frame->project_duration_frames);
    } else if (frame->profile == DATALAB_PROFILE_TRACE) {
        printf("  trace_version=%u samples=%zu markers=%zu\n",
               frame->trace_version,
               frame->trace_sample_count,
               frame->trace_marker_count);
    } else if (frame->profile == DATALAB_PROFILE_SKETCH) {
        printf("  schema=%u logical=%ux%u raster=%ux%u density=%u layers=%u\n",
               frame->drawing_schema_version,
               frame->logical_width,
               frame->logical_height,
               frame->width,
               frame->height,
               frame->sample_density,
               frame->drawing_layer_count);
        printf("  objects=%u rendered=%u unsupported=%u\n",
               frame->drawing_object_count,
               frame->drawing_rendered_object_count,
               frame->drawing_unsupported_object_count);
    }
    if (frame->profile == DATALAB_PROFILE_GROWTH) printf("  growth=%s steps=%u primary_field=%s\n", frame->growth_schema_id, frame->growth_steps_executed, frame->growth_primary_field);
    if (frame->profile == DATALAB_PROFILE_LINE_DIAGNOSTIC) printf("  line schema=%u anchors=%u walls=%u curved=%u persistent=%u preview=xy_anchors_walls\n", frame->line_schema_version, frame->line_anchor_count, frame->line_wall_count, frame->line_curved_anchor_count, frame->line_persistent_anchor_count);

    if (frame->manifest_json) {
        printf("  manifest_bytes=%zu\n", frame->manifest_size);
    }
}
