#include "render/datalab_render_perf_diag.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct DatalabRenderPerfRuntime {
    int initialized;
    int enabled;
    int frame_active;
    uint32_t period_ms;
    uint32_t period_begin_ticks;
    uint64_t frame_begin_counter;
    uint64_t frame_sequence;
    uint64_t previous_raster_upload_count;
    uint64_t previous_raster_upload_bytes;
    uint64_t previous_raster_reuse_count;
    int previous_raster_valid;
    DatalabRenderPerfFrame frame;
    DatalabRenderPerfAccumulator accumulator;
} DatalabRenderPerfRuntime;

static DatalabRenderPerfRuntime g_datalab_render_perf;

static uint64_t datalab_render_perf_add_saturating(uint64_t base, uint64_t delta) {
    return UINT64_MAX - base < delta ? UINT64_MAX : base + delta;
}

static double datalab_render_perf_nonnegative_finite(double value) {
    return isfinite(value) && value > 0.0 ? value : 0.0;
}

static void datalab_render_perf_accumulate_timing(double value,
                                                   double *total,
                                                   double *maximum) {
    value = datalab_render_perf_nonnegative_finite(value);
    *total += value;
    if (value > *maximum) {
        *maximum = value;
    }
}

int datalab_render_perf_rgba_bytes(uint32_t width, uint32_t height, uint64_t *out_bytes) {
    uint64_t pixels;
    if (!out_bytes || width == 0u || height == 0u) {
        return 0;
    }
    pixels = (uint64_t)width * (uint64_t)height;
    if (pixels > UINT64_MAX / 4u) {
        return 0;
    }
    *out_bytes = pixels * 4u;
    return 1;
}

const char *datalab_render_perf_stage_name(DatalabRenderPerfStage stage) {
    switch (stage) {
        case DATALAB_RENDER_PERF_STAGE_SOFTWARE_SUBMIT: return "software_submit";
        case DATALAB_RENDER_PERF_STAGE_COMPATIBILITY_UPLOAD: return "compatibility_upload";
        case DATALAB_RENDER_PERF_STAGE_VULKAN_BEGIN: return "vulkan_begin";
        case DATALAB_RENDER_PERF_STAGE_VULKAN_DRAW: return "vulkan_draw";
        case DATALAB_RENDER_PERF_STAGE_VULKAN_END: return "vulkan_end";
        case DATALAB_RENDER_PERF_STAGE_PRESENT_SYNC: return "present_sync";
        case DATALAB_RENDER_PERF_STAGE_NONE:
        default: return "none";
    }
}

void datalab_render_perf_accumulator_reset(DatalabRenderPerfAccumulator *accumulator) {
    if (accumulator) {
        memset(accumulator, 0, sizeof(*accumulator));
    }
}

void datalab_render_perf_accumulator_note(DatalabRenderPerfAccumulator *accumulator,
                                          const DatalabRenderPerfFrame *frame) {
    if (!accumulator || !frame || frame->schema_version != DATALAB_RENDER_PERF_SCHEMA_VERSION) {
        return;
    }
    accumulator->frame_count = datalab_render_perf_add_saturating(accumulator->frame_count, 1u);
    if (frame->present_succeeded) {
        accumulator->success_count = datalab_render_perf_add_saturating(accumulator->success_count, 1u);
    } else {
        accumulator->failure_count = datalab_render_perf_add_saturating(accumulator->failure_count, 1u);
        accumulator->last_failure_stage = frame->failure_stage;
        accumulator->last_failure_result = frame->failure_result;
    }
    accumulator->render_reason_bits |= frame->render_reason_bits;
    accumulator->raster_upload_count = datalab_render_perf_add_saturating(
        accumulator->raster_upload_count, frame->raster_upload_count);
    accumulator->raster_upload_bytes = datalab_render_perf_add_saturating(
        accumulator->raster_upload_bytes, frame->raster_upload_bytes);
    accumulator->raster_reuse_count = datalab_render_perf_add_saturating(
        accumulator->raster_reuse_count, frame->raster_reuse_count);
    accumulator->compatibility_upload_count = datalab_render_perf_add_saturating(
        accumulator->compatibility_upload_count, frame->compatibility_upload_count);
    accumulator->compatibility_upload_bytes = datalab_render_perf_add_saturating(
        accumulator->compatibility_upload_bytes, frame->compatibility_upload_bytes);
    datalab_render_perf_accumulate_timing(frame->software_submit_ms,
                                          &accumulator->software_submit_ms_total,
                                          &accumulator->software_submit_ms_max);
    datalab_render_perf_accumulate_timing(frame->compatibility_upload_ms,
                                          &accumulator->compatibility_upload_ms_total,
                                          &accumulator->compatibility_upload_ms_max);
    datalab_render_perf_accumulate_timing(frame->vulkan_begin_ms,
                                          &accumulator->vulkan_begin_ms_total,
                                          &accumulator->vulkan_begin_ms_max);
    datalab_render_perf_accumulate_timing(frame->vulkan_draw_ms,
                                          &accumulator->vulkan_draw_ms_total,
                                          &accumulator->vulkan_draw_ms_max);
    datalab_render_perf_accumulate_timing(frame->vulkan_end_ms,
                                          &accumulator->vulkan_end_ms_total,
                                          &accumulator->vulkan_end_ms_max);
    datalab_render_perf_accumulate_timing(frame->total_present_ms,
                                          &accumulator->total_present_ms_total,
                                          &accumulator->total_present_ms_max);
    accumulator->backend_kind = frame->backend_kind;
    accumulator->profile = frame->profile;
    accumulator->drawable_width = frame->drawable_width;
    accumulator->drawable_height = frame->drawable_height;
}

int datalab_render_perf_format_summary_json(const DatalabRenderPerfAccumulator *accumulator,
                                            char *output,
                                            size_t output_capacity) {
    int written;
    double divisor;
    if (!accumulator || !output || output_capacity == 0u || accumulator->frame_count == 0u) {
        return 0;
    }
    divisor = (double)accumulator->frame_count;
    written = snprintf(
        output,
        output_capacity,
        "{\"tag\":\"DatalabRenderPerf\",\"schema\":1,\"type\":\"summary\","
        "\"frames\":%llu,\"successes\":%llu,\"failures\":%llu,"
        "\"reason_bits\":%llu,\"backend\":%d,\"profile\":%d,"
        "\"drawable\":{\"width\":%u,\"height\":%u},"
        "\"raster\":{\"uploads\":%llu,\"bytes\":%llu,\"reuses\":%llu},"
        "\"compatibility\":{\"uploads\":%llu,\"bytes\":%llu},"
        "\"timings_ms\":{"
        "\"software_avg\":%.3f,\"software_max\":%.3f,"
        "\"compat_upload_avg\":%.3f,\"compat_upload_max\":%.3f,"
        "\"vk_begin_avg\":%.3f,\"vk_begin_max\":%.3f,"
        "\"vk_draw_avg\":%.3f,\"vk_draw_max\":%.3f,"
        "\"vk_end_avg\":%.3f,\"vk_end_max\":%.3f,"
        "\"present_avg\":%.3f,\"present_max\":%.3f},"
        "\"last_failure\":{\"stage\":\"%s\",\"result\":%d}}",
        (unsigned long long)accumulator->frame_count,
        (unsigned long long)accumulator->success_count,
        (unsigned long long)accumulator->failure_count,
        (unsigned long long)accumulator->render_reason_bits,
        accumulator->backend_kind,
        accumulator->profile,
        accumulator->drawable_width,
        accumulator->drawable_height,
        (unsigned long long)accumulator->raster_upload_count,
        (unsigned long long)accumulator->raster_upload_bytes,
        (unsigned long long)accumulator->raster_reuse_count,
        (unsigned long long)accumulator->compatibility_upload_count,
        (unsigned long long)accumulator->compatibility_upload_bytes,
        accumulator->software_submit_ms_total / divisor,
        accumulator->software_submit_ms_max,
        accumulator->compatibility_upload_ms_total / divisor,
        accumulator->compatibility_upload_ms_max,
        accumulator->vulkan_begin_ms_total / divisor,
        accumulator->vulkan_begin_ms_max,
        accumulator->vulkan_draw_ms_total / divisor,
        accumulator->vulkan_draw_ms_max,
        accumulator->vulkan_end_ms_total / divisor,
        accumulator->vulkan_end_ms_max,
        accumulator->total_present_ms_total / divisor,
        accumulator->total_present_ms_max,
        datalab_render_perf_stage_name(accumulator->last_failure_stage),
        accumulator->last_failure_result);
    return written > 0 && (size_t)written < output_capacity;
}

static int datalab_render_perf_env_truthy(const char *value) {
    return value && value[0] &&
           (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
            strcasecmp(value, "yes") == 0);
}

static uint32_t datalab_render_perf_period_ms(void) {
    const char *value = getenv("DATALAB_RENDER_PERF_DIAG_PERIOD_MS");
    char *end = NULL;
    unsigned long parsed;
    if (!value || !value[0]) {
        return 1000u;
    }
    parsed = strtoul(value, &end, 10);
    if (!end || end == value || *end != '\0' || parsed < 100u || parsed > 60000u) {
        return 1000u;
    }
    return (uint32_t)parsed;
}

static void datalab_render_perf_diag_init(void) {
    if (g_datalab_render_perf.initialized) {
        return;
    }
    g_datalab_render_perf.enabled = datalab_render_perf_env_truthy(
        getenv("DATALAB_RENDER_PERF_DIAG"));
    g_datalab_render_perf.period_ms = datalab_render_perf_period_ms();
    g_datalab_render_perf.period_begin_ticks = SDL_GetTicks();
    g_datalab_render_perf.initialized = 1;
}

static double datalab_render_perf_elapsed_ms(uint64_t begin_counter) {
    uint64_t frequency;
    uint64_t end_counter;
    if (begin_counter == 0u) {
        return 0.0;
    }
    frequency = SDL_GetPerformanceFrequency();
    end_counter = SDL_GetPerformanceCounter();
    if (frequency == 0u || end_counter < begin_counter) {
        return 0.0;
    }
    return ((double)(end_counter - begin_counter) * 1000.0) / (double)frequency;
}

static uint64_t datalab_render_perf_delta(uint64_t current, uint64_t previous) {
    return current >= previous ? current - previous : current;
}

void datalab_render_perf_diag_begin_frame(int profile,
                                          uint32_t render_reason_bits,
                                          uint32_t source_width,
                                          uint32_t source_height) {
    datalab_render_perf_diag_init();
    if (!g_datalab_render_perf.enabled) {
        return;
    }
    if (g_datalab_render_perf.frame_active) {
        datalab_render_perf_diag_finish(0,
                                        DATALAB_RENDER_PERF_STAGE_SOFTWARE_SUBMIT,
                                        -1);
    }
    memset(&g_datalab_render_perf.frame, 0, sizeof(g_datalab_render_perf.frame));
    g_datalab_render_perf.frame.schema_version = DATALAB_RENDER_PERF_SCHEMA_VERSION;
    g_datalab_render_perf.frame.frame_sequence = ++g_datalab_render_perf.frame_sequence;
    g_datalab_render_perf.frame.render_reason_bits = render_reason_bits;
    g_datalab_render_perf.frame.profile = profile;
    g_datalab_render_perf.frame.source_width = source_width;
    g_datalab_render_perf.frame.source_height = source_height;
    g_datalab_render_perf.frame_begin_counter = SDL_GetPerformanceCounter();
    g_datalab_render_perf.frame_active = 1;
}

uint64_t datalab_render_perf_diag_stage_begin(DatalabRenderPerfStage stage) {
    (void)stage;
    datalab_render_perf_diag_init();
    return g_datalab_render_perf.enabled && g_datalab_render_perf.frame_active
               ? SDL_GetPerformanceCounter()
               : 0u;
}

void datalab_render_perf_diag_stage_end(DatalabRenderPerfStage stage, uint64_t begin_counter) {
    double elapsed;
    if (!g_datalab_render_perf.enabled || !g_datalab_render_perf.frame_active || begin_counter == 0u) {
        return;
    }
    elapsed = datalab_render_perf_elapsed_ms(begin_counter);
    switch (stage) {
        case DATALAB_RENDER_PERF_STAGE_SOFTWARE_SUBMIT:
            g_datalab_render_perf.frame.software_submit_ms += elapsed;
            break;
        case DATALAB_RENDER_PERF_STAGE_COMPATIBILITY_UPLOAD:
            g_datalab_render_perf.frame.compatibility_upload_ms += elapsed;
            break;
        case DATALAB_RENDER_PERF_STAGE_VULKAN_BEGIN:
            g_datalab_render_perf.frame.vulkan_begin_ms += elapsed;
            break;
        case DATALAB_RENDER_PERF_STAGE_VULKAN_DRAW:
            g_datalab_render_perf.frame.vulkan_draw_ms += elapsed;
            break;
        case DATALAB_RENDER_PERF_STAGE_VULKAN_END:
            g_datalab_render_perf.frame.vulkan_end_ms += elapsed;
            break;
        case DATALAB_RENDER_PERF_STAGE_PRESENT_SYNC:
        case DATALAB_RENDER_PERF_STAGE_NONE:
        default:
            break;
    }
}

void datalab_render_perf_diag_note_backend(int backend_kind,
                                           uint32_t drawable_width,
                                           uint32_t drawable_height,
                                           uint32_t canvas_width,
                                           uint32_t canvas_height) {
    if (!g_datalab_render_perf.enabled || !g_datalab_render_perf.frame_active) {
        return;
    }
    g_datalab_render_perf.frame.backend_kind = backend_kind;
    g_datalab_render_perf.frame.drawable_width = drawable_width;
    g_datalab_render_perf.frame.drawable_height = drawable_height;
    g_datalab_render_perf.frame.canvas_width = canvas_width;
    g_datalab_render_perf.frame.canvas_height = canvas_height;
}

void datalab_render_perf_diag_note_raster(uint64_t content_generation,
                                          uint64_t resource_generation,
                                          uint64_t upload_count,
                                          uint64_t upload_byte_count,
                                          uint64_t upload_reuse_count) {
    if (!g_datalab_render_perf.enabled || !g_datalab_render_perf.frame_active) {
        return;
    }
    g_datalab_render_perf.frame.content_generation = content_generation;
    g_datalab_render_perf.frame.resource_generation = resource_generation;
    if (g_datalab_render_perf.previous_raster_valid) {
        g_datalab_render_perf.frame.raster_upload_count = datalab_render_perf_delta(
            upload_count, g_datalab_render_perf.previous_raster_upload_count);
        g_datalab_render_perf.frame.raster_upload_bytes = datalab_render_perf_delta(
            upload_byte_count, g_datalab_render_perf.previous_raster_upload_bytes);
        g_datalab_render_perf.frame.raster_reuse_count = datalab_render_perf_delta(
            upload_reuse_count, g_datalab_render_perf.previous_raster_reuse_count);
    } else {
        g_datalab_render_perf.frame.raster_upload_count = upload_count;
        g_datalab_render_perf.frame.raster_upload_bytes = upload_byte_count;
        g_datalab_render_perf.frame.raster_reuse_count = upload_reuse_count;
        g_datalab_render_perf.previous_raster_valid = 1;
    }
    g_datalab_render_perf.previous_raster_upload_count = upload_count;
    g_datalab_render_perf.previous_raster_upload_bytes = upload_byte_count;
    g_datalab_render_perf.previous_raster_reuse_count = upload_reuse_count;
}

void datalab_render_perf_diag_note_compatibility_upload(uint64_t bytes, int result) {
    if (!g_datalab_render_perf.enabled || !g_datalab_render_perf.frame_active) {
        return;
    }
    g_datalab_render_perf.frame.compatibility_upload_count += 1u;
    g_datalab_render_perf.frame.compatibility_upload_bytes = datalab_render_perf_add_saturating(
        g_datalab_render_perf.frame.compatibility_upload_bytes, bytes);
    if (result != 0) {
        g_datalab_render_perf.frame.failure_stage = DATALAB_RENDER_PERF_STAGE_COMPATIBILITY_UPLOAD;
        g_datalab_render_perf.frame.failure_result = result;
    }
}

void datalab_render_perf_diag_finish(int present_succeeded,
                                     DatalabRenderPerfStage failure_stage,
                                     int failure_result) {
    uint32_t now_ticks;
    if (!g_datalab_render_perf.enabled || !g_datalab_render_perf.frame_active) {
        return;
    }
    g_datalab_render_perf.frame.present_succeeded = present_succeeded ? 1 : 0;
    if (!present_succeeded) {
        g_datalab_render_perf.frame.failure_stage = failure_stage;
        g_datalab_render_perf.frame.failure_result = failure_result;
    }
    g_datalab_render_perf.frame.total_present_ms = datalab_render_perf_elapsed_ms(
        g_datalab_render_perf.frame_begin_counter);
    datalab_render_perf_accumulator_note(&g_datalab_render_perf.accumulator,
                                         &g_datalab_render_perf.frame);
    g_datalab_render_perf.frame_active = 0;
    now_ticks = SDL_GetTicks();
    if (!present_succeeded ||
        (uint32_t)(now_ticks - g_datalab_render_perf.period_begin_ticks) >=
            g_datalab_render_perf.period_ms) {
        datalab_render_perf_diag_flush();
        g_datalab_render_perf.period_begin_ticks = now_ticks;
    }
}

void datalab_render_perf_diag_flush(void) {
    char output[2048];
    if (!g_datalab_render_perf.enabled || g_datalab_render_perf.accumulator.frame_count == 0u) {
        return;
    }
    if (datalab_render_perf_format_summary_json(&g_datalab_render_perf.accumulator,
                                                output,
                                                sizeof(output))) {
        fprintf(stdout, "%s\n", output);
        fflush(stdout);
    }
    datalab_render_perf_accumulator_reset(&g_datalab_render_perf.accumulator);
}
