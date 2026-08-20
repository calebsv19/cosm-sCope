#ifndef DATALAB_PACK_LOADER_H
#define DATALAB_PACK_LOADER_H

#include <stddef.h>
#include <stdint.h>

#include "core_base.h"

typedef enum DatalabProfile {
    DATALAB_PROFILE_UNKNOWN = 0,
    DATALAB_PROFILE_PHYSICS = 1,
    DATALAB_PROFILE_DAW = 2,
    DATALAB_PROFILE_TRACE = 3,
    DATALAB_PROFILE_SKETCH = 4,
    DATALAB_PROFILE_IMAGE = 5,
    DATALAB_PROFILE_VOLUME = 6,
    DATALAB_PROFILE_GROWTH = 7,
    DATALAB_PROFILE_LINE_DIAGNOSTIC = 8
} DatalabProfile;

typedef enum DatalabImageFormat {
    DATALAB_IMAGE_FORMAT_UNKNOWN = 0,
    DATALAB_IMAGE_FORMAT_BMP = 1,
    DATALAB_IMAGE_FORMAT_PNG = 2
} DatalabImageFormat;

typedef enum DatalabImageTransfer {
    DATALAB_IMAGE_TRANSFER_UNKNOWN = 0,
    DATALAB_IMAGE_TRANSFER_UNTAGGED_SRGB_ASSUMED = 1,
    DATALAB_IMAGE_TRANSFER_SRGB = 2,
    DATALAB_IMAGE_TRANSFER_GAMA = 3,
    DATALAB_IMAGE_TRANSFER_ICC_UNTRANSFORMED = 4
} DatalabImageTransfer;

typedef struct DatalabImageMetadata {
    DatalabImageFormat format;
    DatalabImageTransfer transfer;
    uint8_t source_bit_depth;
    uint8_t source_has_alpha;
    uint8_t png_srgb_present;
    uint8_t png_gamma_present;
    uint8_t png_icc_present;
    double png_gamma;
} DatalabImageMetadata;

typedef struct DatalabDawMarker {
    uint64_t frame;
    double beat;
    uint32_t kind;
    uint32_t reserved;
    double value_a;
    double value_b;
} DatalabDawMarker;

typedef struct DatalabTraceSample {
    double time_seconds;
    float value;
    char lane[32];
} DatalabTraceSample;

typedef struct DatalabTraceMarker {
    double time_seconds;
    char lane[32];
    char label[64];
} DatalabTraceMarker;

typedef struct DatalabLineAnchor {
    float x;
    float y;
    uint32_t persistent;
    uint32_t anchor_type;
} DatalabLineAnchor;

typedef struct DatalabLineWall {
    uint32_t anchor_a;
    uint32_t anchor_b;
    uint32_t lock_length;
} DatalabLineWall;

typedef struct DatalabFrame {
    DatalabProfile profile;

    uint32_t vf_version;
    uint32_t width;
    uint32_t height;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float cell_size;
    uint32_t obstacle_mask_crc32;
    uint32_t volume_depth;
    uint32_t volume_slice_index;
    float origin_z;
    float voxel_size;
    char growth_schema_id[64];
    char growth_primary_field[16];
    uint32_t growth_steps_executed;
    uint32_t line_schema_version;
    uint32_t line_anchor_count;
    uint32_t line_wall_count;
    uint32_t line_curved_anchor_count;
    uint32_t line_persistent_anchor_count;
    DatalabLineAnchor *line_anchors;
    DatalabLineWall *line_walls;

    float *density;
    float *velx;
    float *vely;

    uint32_t daw_version;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t samples_per_pixel;
    uint64_t point_count;
    uint64_t start_frame;
    uint64_t end_frame;
    uint64_t project_duration_frames;
    float *wave_min;
    float *wave_max;
    DatalabDawMarker *markers;
    size_t marker_count;

    uint32_t trace_version;
    DatalabTraceSample *trace_samples;
    size_t trace_sample_count;
    DatalabTraceMarker *trace_markers;
    size_t trace_marker_count;

    uint32_t drawing_schema_version;
    uint32_t logical_width;
    uint32_t logical_height;
    uint32_t sample_density;
    uint32_t drawing_layer_count;
    uint32_t drawing_object_count;
    uint32_t drawing_rendered_object_count;
    uint32_t drawing_unsupported_object_count;
    uint8_t *drawing_rgba;
    DatalabImageMetadata image_metadata;
    /* Assigned when this raster becomes the active render-thread frame. */
    uint64_t raster_content_generation;

    char *manifest_json;
    size_t manifest_size;
    size_t chunk_count;
} DatalabFrame;

void datalab_frame_init(DatalabFrame *frame);
void datalab_frame_free(DatalabFrame *frame);
CoreResult datalab_load_pack(const char *pack_path, DatalabFrame *out_frame);
void datalab_print_frame_summary(const char *pack_path, const DatalabFrame *frame);

#endif
