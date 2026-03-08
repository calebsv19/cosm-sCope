#ifndef DATALAB_PACK_LOADER_H
#define DATALAB_PACK_LOADER_H

#include <stddef.h>
#include <stdint.h>

#include "core_base.h"

typedef enum DatalabProfile {
    DATALAB_PROFILE_UNKNOWN = 0,
    DATALAB_PROFILE_PHYSICS = 1,
    DATALAB_PROFILE_DAW = 2,
    DATALAB_PROFILE_TRACE = 3
} DatalabProfile;

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

    char *manifest_json;
    size_t manifest_size;
    size_t chunk_count;
} DatalabFrame;

void datalab_frame_init(DatalabFrame *frame);
void datalab_frame_free(DatalabFrame *frame);
CoreResult datalab_load_pack(const char *pack_path, DatalabFrame *out_frame);
void datalab_print_frame_summary(const char *pack_path, const DatalabFrame *frame);

#endif
