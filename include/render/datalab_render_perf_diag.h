#ifndef DATALAB_RENDER_PERF_DIAG_H
#define DATALAB_RENDER_PERF_DIAG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DATALAB_RENDER_PERF_SCHEMA_VERSION = 2u
};

typedef enum DatalabRenderPerfStage {
    DATALAB_RENDER_PERF_STAGE_NONE = 0,
    DATALAB_RENDER_PERF_STAGE_SOFTWARE_SUBMIT,
    DATALAB_RENDER_PERF_STAGE_COMPATIBILITY_UPLOAD,
    DATALAB_RENDER_PERF_STAGE_VULKAN_BEGIN,
    DATALAB_RENDER_PERF_STAGE_VULKAN_DRAW,
    DATALAB_RENDER_PERF_STAGE_VULKAN_END,
    DATALAB_RENDER_PERF_STAGE_PRESENT_SYNC
} DatalabRenderPerfStage;

typedef struct DatalabRenderPerfFrame {
    uint32_t schema_version;
    uint64_t frame_sequence;
    uint32_t render_reason_bits;
    int backend_kind;
    int profile;
    uint32_t drawable_width;
    uint32_t drawable_height;
    uint32_t canvas_width;
    uint32_t canvas_height;
    uint32_t source_width;
    uint32_t source_height;
    uint64_t content_generation;
    uint64_t resource_generation;
    uint64_t raster_upload_count;
    uint64_t raster_upload_bytes;
    uint64_t raster_reuse_count;
    uint64_t compatibility_upload_count;
    uint64_t compatibility_upload_bytes;
    double software_submit_ms;
    double compatibility_upload_ms;
    double vulkan_begin_ms;
    double vulkan_draw_ms;
    double vulkan_end_ms;
    double present_sync_ms;
    double total_present_ms;
    int present_succeeded;
    DatalabRenderPerfStage failure_stage;
    int failure_result;
} DatalabRenderPerfFrame;

typedef struct DatalabRenderPerfAccumulator {
    uint64_t frame_count;
    uint64_t success_count;
    uint64_t failure_count;
    uint64_t render_reason_bits;
    uint64_t raster_upload_count;
    uint64_t raster_upload_bytes;
    uint64_t raster_reuse_count;
    uint64_t compatibility_upload_count;
    uint64_t compatibility_upload_bytes;
    double software_submit_ms_total;
    double software_submit_ms_max;
    double compatibility_upload_ms_total;
    double compatibility_upload_ms_max;
    double vulkan_begin_ms_total;
    double vulkan_begin_ms_max;
    double vulkan_draw_ms_total;
    double vulkan_draw_ms_max;
    double vulkan_end_ms_total;
    double vulkan_end_ms_max;
    double present_sync_ms_total;
    double present_sync_ms_max;
    double total_present_ms_total;
    double total_present_ms_max;
    int backend_kind;
    int profile;
    uint32_t drawable_width;
    uint32_t drawable_height;
    DatalabRenderPerfStage last_failure_stage;
    int last_failure_result;
} DatalabRenderPerfAccumulator;

int datalab_render_perf_rgba_bytes(uint32_t width, uint32_t height, uint64_t *out_bytes);
const char *datalab_render_perf_stage_name(DatalabRenderPerfStage stage);
void datalab_render_perf_accumulator_reset(DatalabRenderPerfAccumulator *accumulator);
void datalab_render_perf_accumulator_note(DatalabRenderPerfAccumulator *accumulator,
                                          const DatalabRenderPerfFrame *frame);
int datalab_render_perf_format_summary_json(const DatalabRenderPerfAccumulator *accumulator,
                                            char *output,
                                            size_t output_capacity);

void datalab_render_perf_diag_begin_frame(int profile,
                                          uint32_t render_reason_bits,
                                          uint32_t source_width,
                                          uint32_t source_height);
uint64_t datalab_render_perf_diag_stage_begin(DatalabRenderPerfStage stage);
void datalab_render_perf_diag_stage_end(DatalabRenderPerfStage stage, uint64_t begin_counter);
void datalab_render_perf_diag_note_backend(int backend_kind,
                                           uint32_t drawable_width,
                                           uint32_t drawable_height,
                                           uint32_t canvas_width,
                                           uint32_t canvas_height);
void datalab_render_perf_diag_note_raster(uint64_t content_generation,
                                          uint64_t resource_generation,
                                          uint64_t upload_count,
                                          uint64_t upload_byte_count,
                                          uint64_t upload_reuse_count);
void datalab_render_perf_diag_note_compatibility_upload(uint64_t bytes, int result);
void datalab_render_perf_diag_finish(int present_succeeded,
                                     DatalabRenderPerfStage failure_stage,
                                     int failure_result);
int datalab_render_perf_diag_last_frame(DatalabRenderPerfFrame *out_frame);
void datalab_render_perf_diag_flush(void);

#ifdef __cplusplus
}
#endif

#endif
