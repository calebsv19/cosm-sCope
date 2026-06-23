#include "data/pack_loader_sketch.h"

#include <stddef.h>
#include <string.h>

#include "core_pack.h"
#include "data/pack_loader_internal.h"

enum {
    DATALAB_DRAWING_MAX_LAYERS = 16u,
    DATALAB_DRAWING_MAX_DIMENSION = 16384u,
    DATALAB_DRAWING_MAX_RASTER_SAMPLES = 67108864u,
    DATALAB_DRAWING_MAX_OBJECTS = 65536u,
    DATALAB_DRAWING_MAX_CHUNK_BYTES = 268435456u,
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

typedef struct DrawingDocumentMetadataCanonical {
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
} DrawingDocumentMetadataCanonical;

typedef struct DrawingSnapshotLegacyPrefixCanonical {
    uint32_t version;
    uint32_t reserved0;
    uint32_t node_count;
    uint32_t binding_count;
    uint32_t history_count;
    uint32_t history_cursor;
    DrawingDocumentMetadataCanonical document;
} DrawingSnapshotLegacyPrefixCanonical;

typedef struct DrawingSnapshotShellHeaderCanonical {
    uint32_t version;
    uint32_t reserved0;
    uint32_t node_count;
    uint32_t binding_count;
    uint32_t history_count;
    uint32_t history_cursor;
    uint32_t schema_version;
    uint32_t logical_width;
    uint32_t logical_height;
    uint32_t sample_density;
    uint32_t layer_count;
    uint32_t next_layer_id;
    uint32_t raster_width;
    uint32_t raster_height;
    uint32_t raster_sample_count;
} DrawingSnapshotShellHeaderCanonical;

typedef struct DrawingSnapshotShellPrefixCanonical {
    DrawingSnapshotShellHeaderCanonical header;
    DrawingLayerCanonical layers[DATALAB_DRAWING_MAX_LAYERS];
} DrawingSnapshotShellPrefixCanonical;

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

typedef struct DrawingObjectChunkEntryV4Canonical {
    uint32_t object_id;
    uint32_t layer_id;
    uint8_t type;
    uint8_t visible;
    uint8_t locked;
    uint8_t stroke_width;
    uint8_t style_mode;
    uint8_t path_closed;
    uint16_t path_point_count;
    uint16_t reserved0;
    uint32_t stroke_color_value;
    uint32_t fill_color_value;
    int32_t origin_x;
    int32_t origin_y;
    uint32_t width;
    uint32_t height;
    char name[32];
    DrawingPathPointCanonical path_points[128];
} DrawingObjectChunkEntryV4Canonical;

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

enum {
    DATALAB_DRAWING_LAYER_RASTER_CHUNK_VERSION_V1 = 1u,
    DATALAB_DRAWING_LAYER_RASTER_CHUNK_VERSION_V2 = 2u,
    DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V1 = 1u,
    DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V2 = 2u,
    DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V3 = 3u,
    DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V4 = 4u,
    DATALAB_DRAWING_SNAPSHOT_LEGACY_VERSION_V1 = 1u,
    DATALAB_DRAWING_SNAPSHOT_SHELL_VERSION_V2 = 2u,
    DATALAB_DRAWING_SCHEMA_VERSION_TRUE_COLOR = 4u
};

static uint32_t datalab_drawing_pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r << 16u) |
           ((uint32_t)g << 8u) |
           (uint32_t)b |
           ((uint32_t)a << 24u);
}

static uint32_t datalab_drawing_color_value_from_index(uint8_t sample) {
    if (sample < DATALAB_DRAWING_PALETTE_COUNT) {
        return datalab_drawing_pack_rgba(k_datalab_drawing_palette_rgb[sample][0],
                                         k_datalab_drawing_palette_rgb[sample][1],
                                         k_datalab_drawing_palette_rgb[sample][2],
                                         255u);
    }
    return datalab_drawing_pack_rgba(sample, sample, sample, 255u);
}

static uint32_t datalab_drawing_normalize_legacy_sample(uint8_t sample) {
    if (sample == DATALAB_DRAWING_ERASER_VALUE) {
        return 0u;
    }
    return datalab_drawing_color_value_from_index(sample);
}

static uint32_t datalab_drawing_normalize_input_sample(uint32_t sample) {
    if (sample == 0u) {
        return 0u;
    }
    if (sample <= 255u) {
        return datalab_drawing_normalize_legacy_sample((uint8_t)sample);
    }
    return sample;
}

static int datalab_drawing_sample_is_transparent(uint32_t sample) {
    return (((sample >> 24u) & 0xffu) == 0u) ? 1 : 0;
}

static int datalab_drawing_u64_fits_size(uint64_t value) {
    return value <= (uint64_t)SIZE_MAX;
}

static CoreResult datalab_drawing_checked_chunk_size(uint64_t size, size_t *out_size) {
    if (!out_size) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid drawing chunk size request" };
    }
    *out_size = 0u;
    if (size > DATALAB_DRAWING_MAX_CHUNK_BYTES || !datalab_drawing_u64_fits_size(size)) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing chunk exceeds safety limit" };
    }
    *out_size = (size_t)size;
    return core_result_ok();
}

static CoreResult datalab_drawing_checked_raster_bounds(const DrawingDocumentMetadataCanonical *document,
                                                        size_t *out_sample_count,
                                                        size_t *out_rgba_size) {
    uint64_t sample_count64 = 0u;
    if (!document || !out_sample_count || !out_rgba_size) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid drawing bounds request" };
    }
    *out_sample_count = 0u;
    *out_rgba_size = 0u;
    if (document->layer_count == 0u ||
        document->layer_count > DATALAB_DRAWING_MAX_LAYERS ||
        document->raster_width == 0u ||
        document->raster_height == 0u ||
        document->raster_width > DATALAB_DRAWING_MAX_DIMENSION ||
        document->raster_height > DATALAB_DRAWING_MAX_DIMENSION) {
        return (CoreResult){ CORE_ERR_FORMAT, "invalid drawing snapshot bounds" };
    }
    sample_count64 = (uint64_t)document->raster_width * (uint64_t)document->raster_height;
    if (sample_count64 == 0u ||
        sample_count64 > DATALAB_DRAWING_MAX_RASTER_SAMPLES ||
        document->raster_sample_count != sample_count64 ||
        !datalab_drawing_u64_fits_size(sample_count64)) {
        return (CoreResult){ CORE_ERR_FORMAT, "invalid drawing snapshot bounds" };
    }
    if ((size_t)sample_count64 > (SIZE_MAX / sizeof(uint32_t)) / (size_t)document->layer_count ||
        (size_t)sample_count64 > SIZE_MAX / 4u) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing snapshot allocation too large" };
    }
    *out_sample_count = (size_t)sample_count64;
    *out_rgba_size = (size_t)sample_count64 * 4u;
    return core_result_ok();
}

static CoreResult datalab_drawing_checked_object_chunk_shape(uint64_t payload_size,
                                                            uint32_t object_count,
                                                            size_t entry_size) {
    uint64_t expected_entries = 0u;
    uint64_t expected_size = 0u;
    if (entry_size == 0u) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid drawing object entry size" };
    }
    if (object_count > DATALAB_DRAWING_MAX_OBJECTS) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing object count exceeds safety limit" };
    }
    if ((uint64_t)object_count > (UINT64_MAX / (uint64_t)entry_size)) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing object chunk size mismatch" };
    }
    expected_entries = (uint64_t)object_count * (uint64_t)entry_size;
    if (expected_entries > UINT64_MAX - sizeof(DrawingObjectChunkHeaderCanonical)) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing object chunk size mismatch" };
    }
    expected_size = sizeof(DrawingObjectChunkHeaderCanonical) + expected_entries;
    if (payload_size != expected_size) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing object chunk size mismatch" };
    }
    return core_result_ok();
}

static void datalab_drawing_rgba_from_sample(uint32_t sample,
                                             uint8_t *out_r,
                                             uint8_t *out_g,
                                             uint8_t *out_b,
                                             uint8_t *out_a) {
    if (out_r) *out_r = (uint8_t)((sample >> 16u) & 0xffu);
    if (out_g) *out_g = (uint8_t)((sample >> 8u) & 0xffu);
    if (out_b) *out_b = (uint8_t)(sample & 0xffu);
    if (out_a) *out_a = (uint8_t)((sample >> 24u) & 0xffu);
}

static int datalab_drawing_style_includes_fill(uint8_t style_mode) {
    return (style_mode == 1u || style_mode == 2u) ? 1 : 0;
}

static int datalab_drawing_style_includes_outline(uint8_t style_mode) {
    return (style_mode == 1u) ? 0 : 1;
}

static void datalab_drawing_write_sample(uint32_t *layer_samples,
                                         uint32_t width,
                                         uint32_t height,
                                         int32_t sample_x,
                                         int32_t sample_y,
                                         uint32_t value) {
    size_t index = 0u;
    if (!layer_samples || sample_x < 0 || sample_y < 0 ||
        sample_x >= (int32_t)width || sample_y >= (int32_t)height) {
        return;
    }
    index = ((size_t)sample_y * (size_t)width) + (size_t)sample_x;
    layer_samples[index] = value;
}

static void datalab_drawing_rasterize_rect(uint32_t *layer_samples,
                                           uint32_t width,
                                           uint32_t height,
                                           int32_t origin_x,
                                           int32_t origin_y,
                                           uint32_t rect_width,
                                           uint32_t rect_height,
                                           uint32_t fill_value,
                                           uint32_t stroke_value,
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

static void datalab_drawing_rasterize_ellipse(uint32_t *layer_samples,
                                              uint32_t width,
                                              uint32_t height,
                                              int32_t origin_x,
                                              int32_t origin_y,
                                              uint32_t ellipse_width,
                                              uint32_t ellipse_height,
                                              uint32_t fill_value,
                                              uint32_t stroke_value,
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


static CoreResult load_sketch_profile_metadata_from_shell(CorePackReader *reader,
                                                          const CorePackChunkInfo *dps3,
                                                          DrawingSnapshotShellPrefixCanonical *out_shell) {
    if (!reader || !dps3 || !out_shell) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid drawing shell metadata request" };
    }
    if (dps3->size < sizeof(*out_shell)) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing snapshot shell chunk too small" };
    }
    memset(out_shell, 0, sizeof(*out_shell));
    return core_pack_reader_read_chunk_slice(reader, dps3, 0u, out_shell, sizeof(*out_shell));
}

static CoreResult load_sketch_profile_metadata_from_legacy(CorePackReader *reader,
                                                           const CorePackChunkInfo *dps2,
                                                           DrawingSnapshotLegacyPrefixCanonical *out_legacy) {
    if (!reader || !dps2 || !out_legacy) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid legacy drawing metadata request" };
    }
    if (dps2->size < sizeof(*out_legacy)) {
        return (CoreResult){ CORE_ERR_FORMAT, "drawing snapshot chunk too small" };
    }
    memset(out_legacy, 0, sizeof(*out_legacy));
    return core_pack_reader_read_chunk_slice(reader, dps2, 0u, out_legacy, sizeof(*out_legacy));
}

CoreResult datalab_pack_loader_load_sketch_profile(CorePackReader *reader,
                                      const CorePackChunkInfo *dps3,
                                      const CorePackChunkInfo *dps2,
                                      DatalabFrame *out_frame) {
    DrawingSnapshotShellPrefixCanonical shell;
    DrawingSnapshotLegacyPrefixCanonical legacy;
    const DrawingDocumentMetadataCanonical *document = NULL;
    const DrawingLayerCanonical *layers = NULL;
    DrawingLayerRasterChunkHeaderCanonical layer_hdr;
    CorePackChunkInfo dplr;
    CorePackChunkInfo dpob;
    CoreResult rr;
    uint8_t *layer_chunk_data = NULL;
    uint32_t *layer_storage = NULL;
    uint32_t *composed_samples = NULL;
    uint8_t *rgba = NULL;
    size_t sample_count = 0u;
    size_t rgba_size = 0u;
    uint32_t i = 0u;

    if (!reader || (!dps3 && !dps2) || !out_frame) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid sketch loader arguments" };
    }

    memset(&shell, 0, sizeof(shell));
    memset(&legacy, 0, sizeof(legacy));
    if (dps3) {
        rr = load_sketch_profile_metadata_from_shell(reader, dps3, &shell);
        if (rr.code != CORE_OK) {
            return rr;
        }
        if (shell.header.version != DATALAB_DRAWING_SNAPSHOT_SHELL_VERSION_V2) {
            return (CoreResult){ CORE_ERR_FORMAT, "unsupported drawing snapshot shell version" };
        }
        document = (const DrawingDocumentMetadataCanonical *)&shell.header.schema_version;
        layers = shell.layers;
    } else {
        rr = load_sketch_profile_metadata_from_legacy(reader, dps2, &legacy);
        if (rr.code != CORE_OK) {
            return rr;
        }
        if (legacy.version != DATALAB_DRAWING_SNAPSHOT_LEGACY_VERSION_V1) {
            return (CoreResult){ CORE_ERR_FORMAT, "unsupported drawing snapshot version" };
        }
        document = &legacy.document;
        layers = legacy.document.layers;
    }

    rr = datalab_drawing_checked_raster_bounds(document, &sample_count, &rgba_size);
    if (rr.code != CORE_OK) {
        return rr;
    }

    layer_storage = (uint32_t *)core_alloc(sample_count * (size_t)document->layer_count * sizeof(*layer_storage));
    composed_samples = (uint32_t *)core_alloc(sample_count * sizeof(*composed_samples));
    rgba = (uint8_t *)core_alloc(rgba_size);
    if (!layer_storage || !composed_samples || !rgba) {
        rr = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }
    memset(layer_storage, 0, sample_count * (size_t)document->layer_count * sizeof(*layer_storage));

    if (dps2 && document->layer_count > 0u) {
        uint64_t inline_raster_offset = (uint64_t)sizeof(DrawingSnapshotLegacyPrefixCanonical);
        uint64_t inline_sample_bytes = (document->schema_version >= DATALAB_DRAWING_SCHEMA_VERSION_TRUE_COLOR)
                                           ? (uint64_t)sizeof(uint32_t)
                                           : 1u;
        uint64_t inline_raster_size = (uint64_t)document->raster_sample_count * inline_sample_bytes;
        if (inline_raster_offset <= dps2->size &&
            inline_raster_size <= dps2->size - inline_raster_offset) {
            if (inline_sample_bytes == sizeof(uint32_t)) {
                rr = core_pack_reader_read_chunk_slice(reader,
                                                       dps2,
                                                       inline_raster_offset,
                                                       layer_storage,
                                                       inline_raster_size);
                if (rr.code != CORE_OK) {
                    goto cleanup;
                }
                for (i = 0u; i < (uint32_t)sample_count; ++i) {
                    layer_storage[i] = datalab_drawing_normalize_input_sample(layer_storage[i]);
                }
            } else {
                uint8_t *legacy_samples = (uint8_t *)core_alloc((size_t)inline_raster_size);
                if (!legacy_samples) {
                    rr = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                    goto cleanup;
                }
                rr = core_pack_reader_read_chunk_slice(reader,
                                                       dps2,
                                                       inline_raster_offset,
                                                       legacy_samples,
                                                       inline_raster_size);
                if (rr.code != CORE_OK) {
                    core_free(legacy_samples);
                    goto cleanup;
                }
                for (i = 0u; i < (uint32_t)sample_count; ++i) {
                    layer_storage[i] = datalab_drawing_normalize_legacy_sample(legacy_samples[i]);
                }
                core_free(legacy_samples);
            }
        }
    }

    memset(&dplr, 0, sizeof(dplr));
    rr = core_pack_reader_find_chunk(reader, "DPLR", 0u, &dplr);
    if (rr.code == CORE_OK) {
        size_t layer_chunk_size = 0u;
        if (dplr.size < sizeof(layer_hdr)) {
            rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk too small" };
            goto cleanup;
        }
        rr = datalab_drawing_checked_chunk_size(dplr.size, &layer_chunk_size);
        if (rr.code != CORE_OK) {
            goto cleanup;
        }
        memset(&layer_hdr, 0, sizeof(layer_hdr));
        rr = core_pack_reader_read_chunk_slice(reader, &dplr, 0u, &layer_hdr, sizeof(layer_hdr));
        if (rr.code != CORE_OK) {
            goto cleanup;
        }
        if ((layer_hdr.version != DATALAB_DRAWING_LAYER_RASTER_CHUNK_VERSION_V1 &&
             layer_hdr.version != DATALAB_DRAWING_LAYER_RASTER_CHUNK_VERSION_V2) ||
            layer_hdr.raster_width != document->raster_width ||
            layer_hdr.raster_height != document->raster_height ||
            layer_hdr.sample_count != document->raster_sample_count ||
            layer_hdr.layer_count != document->layer_count) {
            rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk shape mismatch" };
            goto cleanup;
        }
        layer_chunk_data = (uint8_t *)core_alloc(layer_chunk_size);
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
            const uint8_t *end = layer_chunk_data + layer_chunk_size;
            uint64_t bytes_per_sample = (layer_hdr.version == DATALAB_DRAWING_LAYER_RASTER_CHUNK_VERSION_V2)
                                            ? (uint64_t)sizeof(uint32_t)
                                            : 1u;
            for (i = 0u; i < document->layer_count; ++i) {
                uint32_t layer_id = 0u;
                uint32_t entry_count = 0u;
                uint32_t layer_index = 0u;
                uint64_t entry_bytes = 0u;
                if ((size_t)(end - cursor) < sizeof(uint32_t) * 2u) {
                    rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk truncated" };
                    goto cleanup;
                }
                memcpy(&layer_id, cursor, sizeof(uint32_t));
                cursor += sizeof(uint32_t);
                memcpy(&entry_count, cursor, sizeof(uint32_t));
                cursor += sizeof(uint32_t);
                entry_bytes = (uint64_t)entry_count * bytes_per_sample;
                if (entry_count != document->raster_sample_count ||
                    (uint64_t)(end - cursor) < entry_bytes) {
                    rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer entry sample mismatch" };
                    goto cleanup;
                }
                for (layer_index = 0u; layer_index < document->layer_count; ++layer_index) {
                    if (layers[layer_index].layer_id == layer_id) {
                        uint32_t *dst = layer_storage + ((size_t)layer_index * sample_count);
                        if (layer_hdr.version == DATALAB_DRAWING_LAYER_RASTER_CHUNK_VERSION_V2) {
                            memcpy(dst, cursor, (size_t)entry_bytes);
                            {
                                size_t sample_index = 0u;
                                for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
                                    dst[sample_index] = datalab_drawing_normalize_input_sample(dst[sample_index]);
                                }
                            }
                        } else {
                            size_t sample_index = 0u;
                            for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
                                dst[sample_index] = datalab_drawing_normalize_legacy_sample(cursor[sample_index]);
                            }
                        }
                        break;
                    }
                }
                cursor += entry_bytes;
            }
            if (cursor != end) {
                rr = (CoreResult){ CORE_ERR_FORMAT, "drawing layer chunk trailing bytes" };
                goto cleanup;
            }
        }
    } else if (rr.code != CORE_ERR_NOT_FOUND) {
        goto cleanup;
    } else {
        rr = core_result_ok();
    }

    memset(&dpob, 0, sizeof(dpob));
    rr = core_pack_reader_find_chunk(reader, "DPOB", 0u, &dpob);
    if (rr.code == CORE_OK) {
        uint8_t *object_chunk_data = NULL;
        DrawingObjectChunkHeaderCanonical object_hdr;
        size_t object_chunk_size = 0u;
        memset(&object_hdr, 0, sizeof(object_hdr));
        if (dpob.size < sizeof(object_hdr)) {
            rr = (CoreResult){ CORE_ERR_FORMAT, "drawing object chunk too small" };
            goto cleanup;
        }
        rr = datalab_drawing_checked_chunk_size(dpob.size, &object_chunk_size);
        if (rr.code != CORE_OK) {
            goto cleanup;
        }
        object_chunk_data = (uint8_t *)core_alloc(object_chunk_size);
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
        if (object_hdr.version == DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V1) {
            const uint8_t *cursor = object_chunk_data + sizeof(object_hdr);
            const size_t entry_size = sizeof(DrawingObjectChunkEntryV1Canonical);
            rr = datalab_drawing_checked_object_chunk_shape(dpob.size, object_hdr.object_count, entry_size);
            if (rr.code != CORE_OK) {
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
                for (layer_index = 0u; layer_index < document->layer_count; ++layer_index) {
                    if (layers[layer_index].layer_id == entry.layer_id) {
                        break;
                    }
                }
                if (layer_index >= document->layer_count) {
                    continue;
                }
                if (entry.type == DATALAB_DRAWING_OBJECT_TYPE_RECT) {
                    datalab_drawing_rasterize_rect(layer_storage + ((size_t)layer_index * sample_count),
                                                  document->raster_width,
                                                  document->raster_height,
                                                  entry.origin_x,
                                                  entry.origin_y,
                                                  entry.width,
                                                  entry.height,
                                                  datalab_drawing_color_value_from_index(entry.fill_color_index),
                                                  datalab_drawing_color_value_from_index(entry.stroke_color_index),
                                                  entry.style_mode,
                                                  entry.stroke_width);
                    out_frame->drawing_rendered_object_count += 1u;
                } else if (entry.type == DATALAB_DRAWING_OBJECT_TYPE_ELLIPSE) {
                    datalab_drawing_rasterize_ellipse(layer_storage + ((size_t)layer_index * sample_count),
                                                     document->raster_width,
                                                     document->raster_height,
                                                     entry.origin_x,
                                                     entry.origin_y,
                                                     entry.width,
                                                     entry.height,
                                                     datalab_drawing_color_value_from_index(entry.fill_color_index),
                                                     datalab_drawing_color_value_from_index(entry.stroke_color_index),
                                                     entry.style_mode,
                                                     entry.stroke_width);
                    out_frame->drawing_rendered_object_count += 1u;
                } else {
                    out_frame->drawing_unsupported_object_count += 1u;
                }
            }
        } else if (object_hdr.version == DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V2 ||
                   object_hdr.version == DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V3 ||
                   object_hdr.version == DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V4) {
            const uint8_t *cursor = object_chunk_data + sizeof(object_hdr);
            size_t entry_size = sizeof(DrawingObjectChunkEntryV2Canonical);
            if (object_hdr.version == DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V3) {
                entry_size = sizeof(DrawingObjectChunkEntryV3Canonical);
            } else if (object_hdr.version == DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V4) {
                entry_size = sizeof(DrawingObjectChunkEntryV4Canonical);
            }
            rr = datalab_drawing_checked_object_chunk_shape(dpob.size, object_hdr.object_count, entry_size);
            if (rr.code != CORE_OK) {
                core_free(object_chunk_data);
                goto cleanup;
            }
            for (i = 0u; i < object_hdr.object_count; ++i) {
                uint32_t layer_id = 0u;
                uint8_t object_type = 0u;
                uint8_t visible = 0u;
                uint8_t stroke_width = 0u;
                uint8_t style_mode = 0u;
                int32_t origin_x = 0;
                int32_t origin_y = 0;
                uint32_t object_width = 0u;
                uint32_t object_height = 0u;
                uint32_t fill_value = 0u;
                uint32_t stroke_value = 0u;
                uint32_t layer_index = 0u;
                if (object_hdr.version == DATALAB_DRAWING_OBJECT_CHUNK_VERSION_V4) {
                    memcpy(&layer_id,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, layer_id),
                           sizeof(layer_id));
                    memcpy(&object_type,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, type),
                           sizeof(object_type));
                    memcpy(&visible,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, visible),
                           sizeof(visible));
                    memcpy(&stroke_width,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, stroke_width),
                           sizeof(stroke_width));
                    memcpy(&style_mode,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, style_mode),
                           sizeof(style_mode));
                    memcpy(&origin_x,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, origin_x),
                           sizeof(origin_x));
                    memcpy(&origin_y,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, origin_y),
                           sizeof(origin_y));
                    memcpy(&object_width,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, width),
                           sizeof(object_width));
                    memcpy(&object_height,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, height),
                           sizeof(object_height));
                    memcpy(&stroke_value,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, stroke_color_value),
                           sizeof(stroke_value));
                    memcpy(&fill_value,
                           cursor + offsetof(DrawingObjectChunkEntryV4Canonical, fill_color_value),
                           sizeof(fill_value));
                    stroke_value = datalab_drawing_normalize_input_sample(stroke_value);
                    fill_value = datalab_drawing_normalize_input_sample(fill_value);
                } else {
                    uint8_t stroke_color_index = 0u;
                    uint8_t fill_color_index = 0u;
                    memcpy(&layer_id,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, layer_id),
                           sizeof(layer_id));
                    memcpy(&object_type,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, type),
                           sizeof(object_type));
                    memcpy(&visible,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, visible),
                           sizeof(visible));
                    memcpy(&stroke_width,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, stroke_width),
                           sizeof(stroke_width));
                    memcpy(&style_mode,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, style_mode),
                           sizeof(style_mode));
                    memcpy(&origin_x,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, origin_x),
                           sizeof(origin_x));
                    memcpy(&origin_y,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, origin_y),
                           sizeof(origin_y));
                    memcpy(&object_width,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, width),
                           sizeof(object_width));
                    memcpy(&object_height,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, height),
                           sizeof(object_height));
                    memcpy(&stroke_color_index,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, stroke_color_index),
                           sizeof(stroke_color_index));
                    memcpy(&fill_color_index,
                           cursor + offsetof(DrawingObjectChunkEntryV3Canonical, fill_color_index),
                           sizeof(fill_color_index));
                    stroke_value = datalab_drawing_color_value_from_index(stroke_color_index);
                    fill_value = datalab_drawing_color_value_from_index(fill_color_index);
                }
                cursor += entry_size;
                if (!visible) {
                    continue;
                }
                for (layer_index = 0u; layer_index < document->layer_count; ++layer_index) {
                    if (layers[layer_index].layer_id == layer_id) {
                        break;
                    }
                }
                if (layer_index >= document->layer_count) {
                    continue;
                }
                if (object_type == DATALAB_DRAWING_OBJECT_TYPE_RECT) {
                    datalab_drawing_rasterize_rect(layer_storage + ((size_t)layer_index * sample_count),
                                                  document->raster_width,
                                                  document->raster_height,
                                                  origin_x,
                                                  origin_y,
                                                  object_width,
                                                  object_height,
                                                  fill_value,
                                                  stroke_value,
                                                  style_mode,
                                                  stroke_width);
                    out_frame->drawing_rendered_object_count += 1u;
                } else if (object_type == DATALAB_DRAWING_OBJECT_TYPE_ELLIPSE) {
                    datalab_drawing_rasterize_ellipse(layer_storage + ((size_t)layer_index * sample_count),
                                                     document->raster_width,
                                                     document->raster_height,
                                                     origin_x,
                                                     origin_y,
                                                     object_width,
                                                     object_height,
                                                     fill_value,
                                                     stroke_value,
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
    } else if (rr.code != CORE_ERR_NOT_FOUND) {
        goto cleanup;
    } else {
        rr = core_result_ok();
    }

    memset(composed_samples, 0, sample_count * sizeof(*composed_samples));
    for (i = 0u; i < document->layer_count; ++i) {
        size_t sample_index = 0u;
        const uint32_t *layer_samples = NULL;
        if (!layers[i].visible) {
            continue;
        }
        layer_samples = layer_storage + ((size_t)i * sample_count);
        for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
            if (!datalab_drawing_sample_is_transparent(layer_samples[sample_index])) {
                composed_samples[sample_index] = layer_samples[sample_index];
            }
        }
    }
    for (i = 0u; i < sample_count; ++i) {
        uint8_t r = 0u;
        uint8_t g = 0u;
        uint8_t b = 0u;
        uint8_t a = 0u;
        datalab_drawing_rgba_from_sample(composed_samples[i], &r, &g, &b, &a);
        rgba[i * 4u + 0u] = r;
        rgba[i * 4u + 1u] = g;
        rgba[i * 4u + 2u] = b;
        rgba[i * 4u + 3u] = a;
    }

    out_frame->profile = DATALAB_PROFILE_SKETCH;
    out_frame->drawing_schema_version = document->schema_version;
    out_frame->logical_width = document->logical_width;
    out_frame->logical_height = document->logical_height;
    out_frame->sample_density = document->sample_density;
    out_frame->drawing_layer_count = document->layer_count;
    out_frame->width = document->raster_width;
    out_frame->height = document->raster_height;
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
