#include "data/pack_loader.h"

#include <stdio.h>
#include <string.h>

#include "core_pack.h"

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

enum {
    DATALAB_DRAWING_MAX_LAYERS = 16u,
    DATALAB_DRAWING_OBJECT_TYPE_RECT = 1u,
    DATALAB_DRAWING_OBJECT_TYPE_ELLIPSE = 2u,
    DATALAB_DRAWING_PALETTE_COUNT = 8u,
    DATALAB_DRAWING_ERASER_VALUE = 244u
};

typedef struct DrawingLayerCanonical {
    uint32_t layer_id;
    char name[32];
    uint8_t visible;
    uint8_t locked;
} DrawingLayerCanonical;

typedef struct DrawingDocumentCanonical {
    uint32_t schema_version;
    uint32_t logical_width;
    uint32_t logical_height;
    uint32_t sample_density;
    uint32_t layer_count;
    uint32_t next_layer_id;
    uint32_t raster_width;
    uint32_t raster_height;
    uint32_t raster_sample_count;
    DrawingLayerCanonical layers[DATALAB_DRAWING_MAX_LAYERS];
    uint8_t raster_samples[1048576u];
} DrawingDocumentCanonical;

typedef struct DrawingSnapshotPrefixCanonical {
    uint32_t version;
    uint32_t reserved0;
    uint32_t node_count;
    uint32_t binding_count;
    uint32_t history_count;
    uint32_t history_cursor;
    DrawingDocumentCanonical document;
} DrawingSnapshotPrefixCanonical;

typedef struct DrawingLayerRasterChunkHeaderCanonical {
    uint32_t version;
    uint32_t raster_width;
    uint32_t raster_height;
    uint32_t sample_count;
    uint32_t layer_count;
} DrawingLayerRasterChunkHeaderCanonical;

typedef struct DrawingObjectChunkHeaderCanonical {
    uint32_t version;
    uint32_t object_count;
    uint32_t next_object_id;
    uint32_t reserved0;
} DrawingObjectChunkHeaderCanonical;

typedef struct DrawingPathPointCanonical {
    int32_t x;
    int32_t y;
    int32_t handle_in_dx;
    int32_t handle_in_dy;
    int32_t handle_out_dx;
    int32_t handle_out_dy;
    uint8_t bezier_enabled;
    uint8_t handle_linked;
    uint8_t reserved0;
    uint8_t reserved1;
} DrawingPathPointCanonical;

typedef struct DrawingObjectChunkEntryV1Canonical {
    uint32_t object_id;
    uint32_t layer_id;
    uint8_t type;
    uint8_t visible;
    uint8_t locked;
    uint8_t stroke_color_index;
    uint8_t fill_color_index;
    uint8_t stroke_width;
    uint8_t style_mode;
    uint8_t reserved0;
    uint8_t reserved1;
    int32_t origin_x;
    int32_t origin_y;
    uint32_t width;
    uint32_t height;
    char name[32];
} DrawingObjectChunkEntryV1Canonical;

typedef struct DrawingObjectChunkEntryV2Canonical {
    uint32_t object_id;
    uint32_t layer_id;
    uint8_t type;
    uint8_t visible;
    uint8_t locked;
    uint8_t stroke_color_index;
    uint8_t fill_color_index;
    uint8_t stroke_width;
    uint8_t style_mode;
    uint8_t path_closed;
    uint16_t path_point_count;
    int32_t origin_x;
    int32_t origin_y;
    uint32_t width;
    uint32_t height;
    char name[32];
    struct {
        int32_t x;
        int32_t y;
    } path_points[128];
} DrawingObjectChunkEntryV2Canonical;

typedef struct DrawingObjectChunkEntryV3Canonical {
    uint32_t object_id;
    uint32_t layer_id;
    uint8_t type;
    uint8_t visible;
    uint8_t locked;
    uint8_t stroke_color_index;
    uint8_t fill_color_index;
    uint8_t stroke_width;
    uint8_t style_mode;
    uint8_t path_closed;
    uint16_t path_point_count;
    int32_t origin_x;
    int32_t origin_y;
    uint32_t width;
    uint32_t height;
    char name[32];
    DrawingPathPointCanonical path_points[128];
} DrawingObjectChunkEntryV3Canonical;

static const uint8_t k_datalab_drawing_palette_rgb[DATALAB_DRAWING_PALETTE_COUNT][3] = {
    { 24u, 24u, 24u },
    { 48u, 48u, 48u },
    { 72u, 72u, 72u },
    { 104u, 104u, 104u },
    { 136u, 136u, 136u },
    { 168u, 168u, 168u },
    { 200u, 200u, 200u },
    { 232u, 232u, 232u }
};

static void datalab_drawing_color_rgb_from_sample(uint8_t sample,
                                                  uint8_t *out_r,
                                                  uint8_t *out_g,
                                                  uint8_t *out_b) {
    if (sample < DATALAB_DRAWING_PALETTE_COUNT) {
        if (out_r) *out_r = k_datalab_drawing_palette_rgb[sample][0];
        if (out_g) *out_g = k_datalab_drawing_palette_rgb[sample][1];
        if (out_b) *out_b = k_datalab_drawing_palette_rgb[sample][2];
        return;
    }
    if (out_r) *out_r = sample;
    if (out_g) *out_g = sample;
    if (out_b) *out_b = sample;
}

static int datalab_drawing_style_includes_fill(uint8_t style_mode) {
    return (style_mode == 1u || style_mode == 2u) ? 1 : 0;
}

static int datalab_drawing_style_includes_outline(uint8_t style_mode) {
    return (style_mode == 1u) ? 0 : 1;
}

static void datalab_drawing_write_sample(uint8_t *layer_samples,
                                         uint32_t width,
                                         uint32_t height,
                                         int32_t sample_x,
                                         int32_t sample_y,
                                         uint8_t value) {
    size_t index = 0u;
    if (!layer_samples || sample_x < 0 || sample_y < 0 ||
        sample_x >= (int32_t)width || sample_y >= (int32_t)height) {
        return;
    }
    index = ((size_t)sample_y * (size_t)width) + (size_t)sample_x;
    layer_samples[index] = value;
}

static void datalab_drawing_rasterize_rect(uint8_t *layer_samples,
                                           uint32_t width,
                                           uint32_t height,
                                           int32_t origin_x,
                                           int32_t origin_y,
                                           uint32_t rect_width,
                                           uint32_t rect_height,
                                           uint8_t fill_value,
                                           uint8_t stroke_value,
                                           uint8_t style_mode,
                                           uint8_t stroke_width) {
    uint32_t x = 0u;
    uint32_t y = 0u;
    if (!layer_samples || rect_width == 0u || rect_height == 0u) {
        return;
    }
    if (stroke_width == 0u) {
        stroke_width = 1u;
    }
    if (datalab_drawing_style_includes_fill(style_mode)) {
        for (y = 0u; y < rect_height; ++y) {
            for (x = 0u; x < rect_width; ++x) {
                datalab_drawing_write_sample(layer_samples,
                                             width,
                                             height,
                                             origin_x + (int32_t)x,
                                             origin_y + (int32_t)y,
                                             fill_value);
            }
        }
    }
    if (datalab_drawing_style_includes_outline(style_mode)) {
        uint32_t pass = 0u;
        for (pass = 0u; pass < (uint32_t)stroke_width; ++pass) {
            int32_t left = origin_x + (int32_t)pass;
            int32_t top = origin_y + (int32_t)pass;
            int32_t right = origin_x + (int32_t)rect_width - 1 - (int32_t)pass;
            int32_t bottom = origin_y + (int32_t)rect_height - 1 - (int32_t)pass;
            int32_t draw_x = 0;
            int32_t draw_y = 0;
            if (left > right || top > bottom) {
                break;
            }
            for (draw_x = left; draw_x <= right; ++draw_x) {
                datalab_drawing_write_sample(layer_samples, width, height, draw_x, top, stroke_value);
                if (bottom != top) {
                    datalab_drawing_write_sample(layer_samples, width, height, draw_x, bottom, stroke_value);
                }
            }
            for (draw_y = top + 1; draw_y < bottom; ++draw_y) {
                datalab_drawing_write_sample(layer_samples, width, height, left, draw_y, stroke_value);
                if (right != left) {
                    datalab_drawing_write_sample(layer_samples, width, height, right, draw_y, stroke_value);
                }
            }
        }
    }
}

static void datalab_drawing_rasterize_ellipse(uint8_t *layer_samples,
                                              uint32_t width,
                                              uint32_t height,
                                              int32_t origin_x,
                                              int32_t origin_y,
                                              uint32_t ellipse_width,
                                              uint32_t ellipse_height,
                                              uint8_t fill_value,
                                              uint8_t stroke_value,
                                              uint8_t style_mode,
                                              uint8_t stroke_width) {
    int32_t x = 0;
    int32_t y = 0;
    double rx = 0.0;
    double ry = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double min_radius = 0.0;
    double thickness_norm = 0.0;
    double inner_threshold = 0.0;
    if (!layer_samples || ellipse_width == 0u || ellipse_height == 0u) {
        return;
    }
    rx = ((double)ellipse_width) * 0.5;
    ry = ((double)ellipse_height) * 0.5;
    if (rx <= 0.0 || ry <= 0.0) {
        return;
    }
    if (stroke_width == 0u) {
        stroke_width = 1u;
    }
    cx = (double)origin_x + rx;
    cy = (double)origin_y + ry;
    min_radius = (rx < ry) ? rx : ry;
    thickness_norm = (double)stroke_width / (min_radius > 1.0 ? min_radius : 1.0);
    if (thickness_norm > 1.0) {
        thickness_norm = 1.0;
    }
    inner_threshold = 1.0 - thickness_norm;
    if (inner_threshold < 0.0) {
        inner_threshold = 0.0;
    }
    for (y = origin_y; y < origin_y + (int32_t)ellipse_height; ++y) {
        for (x = origin_x; x < origin_x + (int32_t)ellipse_width; ++x) {
            double nx = (((double)x + 0.5) - cx) / rx;
            double ny = (((double)y + 0.5) - cy) / ry;
            double d = (nx * nx) + (ny * ny);
            if (datalab_drawing_style_includes_outline(style_mode) && d <= 1.0 && d >= inner_threshold) {
                datalab_drawing_write_sample(layer_samples, width, height, x, y, stroke_value);
            } else if (datalab_drawing_style_includes_fill(style_mode) && d <= 1.0) {
                datalab_drawing_write_sample(layer_samples, width, height, x, y, fill_value);
            }
        }
    }
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

    const size_t expected_bytes = expected_count * sizeof(float);
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

    size_t count = (size_t)(chunk->size / sizeof(DatalabDawMarker));
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

static CoreResult read_optional_manifest(CorePackReader *reader, DatalabFrame *out_frame) {
    CorePackChunkInfo json;
    CoreResult fj = core_pack_reader_find_chunk(reader, "JSON", 0, &json);
    if (fj.code != CORE_OK || json.size == 0) {
        return core_result_ok();
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

    const size_t count = (size_t)out_frame->width * (size_t)out_frame->height;

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

    return read_optional_manifest(reader, out_frame);
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

    if (hdr.point_count == 0) {
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

    return read_optional_manifest(reader, out_frame);
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
    out_frame->trace_sample_count = (size_t)hdr.sample_count;
    out_frame->trace_marker_count = (size_t)hdr.marker_count;

    if (hdr.sample_count > 0u) {
        rr = core_pack_reader_find_chunk(reader, "TRSM", 0, &trsm);
        if (rr.code != CORE_OK) {
            CoreResult r = { CORE_ERR_NOT_FOUND, "required trace samples chunk missing" };
            return r;
        }
        if (trsm.size != hdr.sample_count * sizeof(DatalabTraceSample)) {
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
        if (trev.size != hdr.marker_count * sizeof(DatalabTraceMarker)) {
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

    return read_optional_manifest(reader, out_frame);
}

static CoreResult load_sketch_profile(CorePackReader *reader,
                                      const CorePackChunkInfo *dps2,
                                      DatalabFrame *out_frame) {
    DrawingSnapshotPrefixCanonical prefix;
    DrawingLayerRasterChunkHeaderCanonical layer_hdr;
    CorePackChunkInfo dplr;
    CorePackChunkInfo dpob;
    CoreResult rr;
    uint8_t *layer_chunk_data = NULL;
    uint8_t *layer_storage = NULL;
    uint8_t *composed_samples = NULL;
    uint8_t *rgba = NULL;
    size_t sample_count = 0u;
    size_t rgba_size = 0u;
    uint32_t i = 0u;
    if (!reader || !dps2 || !out_frame) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid sketch loader arguments" };
    }
    if (dps2->size < sizeof(prefix)) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing snapshot chunk too small" };
    }
    memset(&prefix, 0, sizeof(prefix));
    rr = core_pack_reader_read_chunk_slice(reader, dps2, 0u, &prefix, sizeof(prefix));
    if (rr.code != CORE_OK) {
        return rr;
    }
    if (prefix.version != 1u) {
        return (CoreResult){ CORE_ERR_FORMAT, "unsupported drawing snapshot version" };
    }
    if (prefix.document.layer_count == 0u ||
        prefix.document.layer_count > DATALAB_DRAWING_MAX_LAYERS ||
        prefix.document.raster_width == 0u ||
        prefix.document.raster_height == 0u ||
        prefix.document.raster_sample_count == 0u ||
        prefix.document.raster_sample_count > 1048576u) {
        return (CoreResult){ CORE_ERR_FORMAT, "invalid drawing snapshot bounds" };
    }
    sample_count = (size_t)prefix.document.raster_sample_count;
    rgba_size = sample_count * 4u;
    if (prefix.document.raster_width != out_frame->width) {
        out_frame->width = prefix.document.raster_width;
    }
    if (prefix.document.raster_height != out_frame->height) {
        out_frame->height = prefix.document.raster_height;
    }

    layer_storage = (uint8_t *)core_alloc(sample_count * (size_t)prefix.document.layer_count);
    composed_samples = (uint8_t *)core_alloc(sample_count);
    rgba = (uint8_t *)core_alloc(rgba_size);
    if (!layer_storage || !composed_samples || !rgba) {
        rr = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }
    memset(layer_storage, DATALAB_DRAWING_ERASER_VALUE, sample_count * (size_t)prefix.document.layer_count);
    memcpy(layer_storage, prefix.document.raster_samples, sample_count);

    memset(&dplr, 0, sizeof(dplr));
    rr = core_pack_reader_find_chunk(reader, "DPLR", 0u, &dplr);
    if (rr.code == CORE_OK) {
        if (dplr.size < sizeof(layer_hdr)) {
            rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk too small" };
            goto cleanup;
        }
        memset(&layer_hdr, 0, sizeof(layer_hdr));
        rr = core_pack_reader_read_chunk_slice(reader, &dplr, 0u, &layer_hdr, sizeof(layer_hdr));
        if (rr.code != CORE_OK) {
            goto cleanup;
        }
        if (layer_hdr.version != 1u ||
            layer_hdr.raster_width != prefix.document.raster_width ||
            layer_hdr.raster_height != prefix.document.raster_height ||
            layer_hdr.sample_count != prefix.document.raster_sample_count ||
            layer_hdr.layer_count != prefix.document.layer_count) {
            rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk shape mismatch" };
            goto cleanup;
        }
        layer_chunk_data = (uint8_t *)core_alloc((size_t)dplr.size);
        if (!layer_chunk_data) {
            rr = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto cleanup;
        }
        rr = core_pack_reader_read_chunk_data(reader, &dplr, layer_chunk_data, dplr.size);
        if (rr.code != CORE_OK) {
            goto cleanup;
        }
        {
            const uint8_t *cursor = layer_chunk_data + sizeof(layer_hdr);
            const uint8_t *end = layer_chunk_data + dplr.size;
            for (i = 0u; i < prefix.document.layer_count; ++i) {
                uint32_t layer_id = 0u;
                uint32_t entry_count = 0u;
                uint32_t layer_index = 0u;
                if ((size_t)(end - cursor) < sizeof(uint32_t) * 2u) {
                    rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk truncated" };
                    goto cleanup;
                }
                memcpy(&layer_id, cursor, sizeof(uint32_t));
                cursor += sizeof(uint32_t);
                memcpy(&entry_count, cursor, sizeof(uint32_t));
                cursor += sizeof(uint32_t);
                if (entry_count != prefix.document.raster_sample_count ||
                    (size_t)(end - cursor) < (size_t)entry_count) {
                    rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer entry sample mismatch" };
                    goto cleanup;
                }
                for (layer_index = 0u; layer_index < prefix.document.layer_count; ++layer_index) {
                    if (prefix.document.layers[layer_index].layer_id == layer_id) {
                        memcpy(layer_storage + ((size_t)layer_index * sample_count), cursor, sample_count);
                        break;
                    }
                }
                cursor += entry_count;
            }
            if (cursor != end) {
                rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk trailing bytes" };
                goto cleanup;
            }
        }
    } else {
        rr = core_result_ok();
    }

    memset(&dpob, 0, sizeof(dpob));
    rr = core_pack_reader_find_chunk(reader, "DPOB", 0u, &dpob);
    if (rr.code == CORE_OK) {
        uint8_t *object_chunk_data = NULL;
        DrawingObjectChunkHeaderCanonical object_hdr;
        memset(&object_hdr, 0, sizeof(object_hdr));
        if (dpob.size < sizeof(object_hdr)) {
            rr = (CoreResult){ CORE_ERR_FORMAT, "drawing object chunk too small" };
            goto cleanup;
        }
        object_chunk_data = (uint8_t *)core_alloc((size_t)dpob.size);
        if (!object_chunk_data) {
            rr = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto cleanup;
        }
        rr = core_pack_reader_read_chunk_data(reader, &dpob, object_chunk_data, dpob.size);
        if (rr.code != CORE_OK) {
            core_free(object_chunk_data);
            goto cleanup;
        }
        memcpy(&object_hdr, object_chunk_data, sizeof(object_hdr));
        out_frame->drawing_object_count = object_hdr.object_count;
        if (object_hdr.version == 1u) {
            const uint8_t *cursor = object_chunk_data + sizeof(object_hdr);
            const uint8_t *end = object_chunk_data + dpob.size;
            const size_t entry_size = sizeof(DrawingObjectChunkEntryV1Canonical);
            if ((size_t)(end - cursor) != entry_size * (size_t)object_hdr.object_count) {
                rr = (CoreResult){ CORE_ERR_FORMAT, "drawing object chunk size mismatch" };
                core_free(object_chunk_data);
                goto cleanup;
            }
            for (i = 0u; i < object_hdr.object_count; ++i) {
                DrawingObjectChunkEntryV1Canonical entry;
                uint32_t layer_index = 0u;
                memcpy(&entry, cursor, sizeof(entry));
                cursor += sizeof(entry);
                if (!entry.visible) {
                    continue;
                }
                for (layer_index = 0u; layer_index < prefix.document.layer_count; ++layer_index) {
                    if (prefix.document.layers[layer_index].layer_id == entry.layer_id) {
                        break;
                    }
                }
                if (layer_index >= prefix.document.layer_count) {
                    continue;
                }
                if (entry.type == DATALAB_DRAWING_OBJECT_TYPE_RECT) {
                    datalab_drawing_rasterize_rect(layer_storage + ((size_t)layer_index * sample_count),
                                                  prefix.document.raster_width,
                                                  prefix.document.raster_height,
                                                  entry.origin_x,
                                                  entry.origin_y,
                                                  entry.width,
                                                  entry.height,
                                                  entry.fill_color_index,
                                                  entry.stroke_color_index,
                                                  entry.style_mode,
                                                  entry.stroke_width);
                    out_frame->drawing_rendered_object_count += 1u;
                } else if (entry.type == DATALAB_DRAWING_OBJECT_TYPE_ELLIPSE) {
                    datalab_drawing_rasterize_ellipse(layer_storage + ((size_t)layer_index * sample_count),
                                                     prefix.document.raster_width,
                                                     prefix.document.raster_height,
                                                     entry.origin_x,
                                                     entry.origin_y,
                                                     entry.width,
                                                     entry.height,
                                                     entry.fill_color_index,
                                                     entry.stroke_color_index,
                                                     entry.style_mode,
                                                     entry.stroke_width);
                    out_frame->drawing_rendered_object_count += 1u;
                } else {
                    out_frame->drawing_unsupported_object_count += 1u;
                }
            }
        } else if (object_hdr.version == 2u || object_hdr.version == 3u) {
            const uint8_t *cursor = object_chunk_data + sizeof(object_hdr);
            const uint8_t *end = object_chunk_data + dpob.size;
            const size_t entry_size = object_hdr.version == 2u
                                          ? sizeof(DrawingObjectChunkEntryV2Canonical)
                                          : sizeof(DrawingObjectChunkEntryV3Canonical);
            if ((size_t)(end - cursor) != entry_size * (size_t)object_hdr.object_count) {
                rr = (CoreResult){ CORE_ERR_FORMAT, "drawing object chunk size mismatch" };
                core_free(object_chunk_data);
                goto cleanup;
            }
            for (i = 0u; i < object_hdr.object_count; ++i) {
                uint32_t layer_id = 0u;
                uint8_t object_type = 0u;
                uint8_t visible = 0u;
                uint8_t stroke_color_index = 0u;
                uint8_t fill_color_index = 0u;
                uint8_t stroke_width = 0u;
                uint8_t style_mode = 0u;
                int32_t origin_x = 0;
                int32_t origin_y = 0;
                uint32_t object_width = 0u;
                uint32_t object_height = 0u;
                uint32_t layer_index = 0u;
                memcpy(&layer_id, cursor + offsetof(DrawingObjectChunkEntryV3Canonical, layer_id), sizeof(layer_id));
                memcpy(&object_type, cursor + offsetof(DrawingObjectChunkEntryV3Canonical, type), sizeof(object_type));
                memcpy(&visible, cursor + offsetof(DrawingObjectChunkEntryV3Canonical, visible), sizeof(visible));
                memcpy(&stroke_color_index,
                       cursor + offsetof(DrawingObjectChunkEntryV3Canonical, stroke_color_index),
                       sizeof(stroke_color_index));
                memcpy(&fill_color_index,
                       cursor + offsetof(DrawingObjectChunkEntryV3Canonical, fill_color_index),
                       sizeof(fill_color_index));
                memcpy(&stroke_width,
                       cursor + offsetof(DrawingObjectChunkEntryV3Canonical, stroke_width),
                       sizeof(stroke_width));
                memcpy(&style_mode, cursor + offsetof(DrawingObjectChunkEntryV3Canonical, style_mode), sizeof(style_mode));
                memcpy(&origin_x, cursor + offsetof(DrawingObjectChunkEntryV3Canonical, origin_x), sizeof(origin_x));
                memcpy(&origin_y, cursor + offsetof(DrawingObjectChunkEntryV3Canonical, origin_y), sizeof(origin_y));
                memcpy(&object_width,
                       cursor + offsetof(DrawingObjectChunkEntryV3Canonical, width),
                       sizeof(object_width));
                memcpy(&object_height,
                       cursor + offsetof(DrawingObjectChunkEntryV3Canonical, height),
                       sizeof(object_height));
                cursor += entry_size;
                if (!visible) {
                    continue;
                }
                for (layer_index = 0u; layer_index < prefix.document.layer_count; ++layer_index) {
                    if (prefix.document.layers[layer_index].layer_id == layer_id) {
                        break;
                    }
                }
                if (layer_index >= prefix.document.layer_count) {
                    continue;
                }
                if (object_type == DATALAB_DRAWING_OBJECT_TYPE_RECT) {
                    datalab_drawing_rasterize_rect(layer_storage + ((size_t)layer_index * sample_count),
                                                  prefix.document.raster_width,
                                                  prefix.document.raster_height,
                                                  origin_x,
                                                  origin_y,
                                                  object_width,
                                                  object_height,
                                                  fill_color_index,
                                                  stroke_color_index,
                                                  style_mode,
                                                  stroke_width);
                    out_frame->drawing_rendered_object_count += 1u;
                } else if (object_type == DATALAB_DRAWING_OBJECT_TYPE_ELLIPSE) {
                    datalab_drawing_rasterize_ellipse(layer_storage + ((size_t)layer_index * sample_count),
                                                     prefix.document.raster_width,
                                                     prefix.document.raster_height,
                                                     origin_x,
                                                     origin_y,
                                                     object_width,
                                                     object_height,
                                                     fill_color_index,
                                                     stroke_color_index,
                                                     style_mode,
                                                     stroke_width);
                    out_frame->drawing_rendered_object_count += 1u;
                } else {
                    out_frame->drawing_unsupported_object_count += 1u;
                }
            }
        } else {
            out_frame->drawing_unsupported_object_count = object_hdr.object_count;
        }
        core_free(object_chunk_data);
    } else {
        rr = core_result_ok();
    }

    memset(composed_samples, DATALAB_DRAWING_ERASER_VALUE, sample_count);
    for (i = 0u; i < prefix.document.layer_count; ++i) {
        size_t sample_index = 0u;
        const uint8_t *layer_samples = NULL;
        if (!prefix.document.layers[i].visible) {
            continue;
        }
        layer_samples = layer_storage + ((size_t)i * sample_count);
        for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
            if (layer_samples[sample_index] != DATALAB_DRAWING_ERASER_VALUE) {
                composed_samples[sample_index] = layer_samples[sample_index];
            }
        }
    }
    for (i = 0u; i < sample_count; ++i) {
        uint8_t r = 0u;
        uint8_t g = 0u;
        uint8_t b = 0u;
        uint8_t a = 255u;
        if (composed_samples[i] == DATALAB_DRAWING_ERASER_VALUE) {
            a = 0u;
        } else {
            datalab_drawing_color_rgb_from_sample(composed_samples[i], &r, &g, &b);
        }
        rgba[i * 4u + 0u] = r;
        rgba[i * 4u + 1u] = g;
        rgba[i * 4u + 2u] = b;
        rgba[i * 4u + 3u] = a;
    }

    out_frame->profile = DATALAB_PROFILE_SKETCH;
    out_frame->drawing_schema_version = prefix.document.schema_version;
    out_frame->logical_width = prefix.document.logical_width;
    out_frame->logical_height = prefix.document.logical_height;
    out_frame->sample_density = prefix.document.sample_density;
    out_frame->drawing_layer_count = prefix.document.layer_count;
    out_frame->width = prefix.document.raster_width;
    out_frame->height = prefix.document.raster_height;
    out_frame->drawing_rgba = rgba;
    rgba = NULL;
    rr = core_result_ok();

cleanup:
    core_free(layer_chunk_data);
    core_free(layer_storage);
    core_free(composed_samples);
    core_free(rgba);
    return rr;
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
    CorePackChunkInfo dawh;
    CorePackChunkInfo trhd;
    CorePackChunkInfo dps2;
    CoreResult find_vfhd = core_pack_reader_find_chunk(&reader, "VFHD", 0, &vfhd);
    CoreResult find_dawh = core_pack_reader_find_chunk(&reader, "DAWH", 0, &dawh);
    CoreResult find_trhd = core_pack_reader_find_chunk(&reader, "TRHD", 0, &trhd);
    CoreResult find_dps2 = core_pack_reader_find_chunk(&reader, "DPS2", 0, &dps2);

    CoreResult load_r;
    if (find_trhd.code == CORE_OK) {
        load_r = load_trace_profile(&reader, &trhd, out_frame);
    } else if (find_vfhd.code == CORE_OK) {
        load_r = load_physics_profile(&reader, &vfhd, out_frame);
    } else if (find_dawh.code == CORE_OK) {
        load_r = load_daw_profile(&reader, &dawh, out_frame);
    } else if (find_dps2.code == CORE_OK) {
        load_r = load_sketch_profile(&reader, &dps2, out_frame);
    } else {
        load_r.code = CORE_ERR_NOT_FOUND;
        load_r.message = "unsupported pack profile (missing TRHD/VFHD/DAWH/DPS2)";
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
           frame->profile == DATALAB_PROFILE_SKETCH ? "sketch" : "unknown",
           frame->chunk_count);

    if (frame->profile == DATALAB_PROFILE_PHYSICS) {
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

    if (frame->manifest_json) {
        printf("  manifest_bytes=%zu\n", frame->manifest_size);
    }
}
