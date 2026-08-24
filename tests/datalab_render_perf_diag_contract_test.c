#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "render/datalab_render_perf_diag.h"

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "render-perf-diag-contract: %s\n", message);
    }
    return condition;
}

int main(void) {
    DatalabRenderPerfAccumulator accumulator;
    DatalabRenderPerfFrame initial;
    DatalabRenderPerfFrame zoom;
    DatalabRenderPerfFrame failure;
    uint64_t bytes = 0u;
    char json[2048];

    if (!require(datalab_render_perf_rgba_bytes(2400u, 1800u, &bytes) &&
                     bytes == 17280000u,
                 "drawable byte count must be exact") ||
        !require(!datalab_render_perf_rgba_bytes(0u, 1800u, &bytes),
                 "zero dimensions must be rejected") ||
        !require(!datalab_render_perf_rgba_bytes(UINT_MAX, UINT_MAX, &bytes),
                 "overflowing dimensions must be rejected")) {
        return 1;
    }

    memset(&initial, 0, sizeof(initial));
    initial.schema_version = DATALAB_RENDER_PERF_SCHEMA_VERSION;
    initial.present_succeeded = 1;
    initial.backend_kind = 1;
    initial.profile = 3;
    initial.drawable_width = 2400u;
    initial.drawable_height = 1800u;
    initial.render_reason_bits = 1u;
    initial.raster_upload_count = 1u;
    initial.raster_upload_bytes = 17280000u;
    initial.compatibility_upload_count = 1u;
    initial.compatibility_upload_bytes = 17280000u;
    initial.software_submit_ms = 8.0;
    initial.compatibility_upload_ms = 5.0;
    initial.total_present_ms = 14.0;

    zoom = initial;
    zoom.render_reason_bits = 2u;
    zoom.raster_upload_count = 0u;
    zoom.raster_upload_bytes = 0u;
    zoom.raster_reuse_count = 1u;
    zoom.software_submit_ms = 10.0;
    zoom.compatibility_upload_ms = 7.0;
    zoom.total_present_ms = 18.0;

    failure = zoom;
    failure.present_succeeded = 0;
    failure.failure_stage = DATALAB_RENDER_PERF_STAGE_VULKAN_END;
    failure.failure_result = -4;
    failure.compatibility_upload_count = 0u;
    failure.compatibility_upload_bytes = 0u;
    failure.total_present_ms = 20.0;

    datalab_render_perf_accumulator_reset(&accumulator);
    datalab_render_perf_accumulator_note(&accumulator, &initial);
    datalab_render_perf_accumulator_note(&accumulator, &zoom);
    datalab_render_perf_accumulator_note(&accumulator, &failure);
    if (!require(accumulator.frame_count == 3u && accumulator.success_count == 2u &&
                     accumulator.failure_count == 1u,
                 "frame outcomes must aggregate independently") ||
        !require(accumulator.raster_upload_count == 1u &&
                     accumulator.raster_upload_bytes == 17280000u &&
                     accumulator.raster_reuse_count == 2u,
                 "content-stable zoom must reuse the source raster") ||
        !require(accumulator.compatibility_upload_count == 2u &&
                     accumulator.compatibility_upload_bytes == 34560000u,
                 "full-window compatibility uploads must remain separately visible") ||
        !require(accumulator.software_submit_ms_max == 10.0 &&
                     accumulator.compatibility_upload_ms_max == 7.0 &&
                     accumulator.total_present_ms_max == 20.0,
                 "maximum timings must be retained") ||
        !require(accumulator.last_failure_stage == DATALAB_RENDER_PERF_STAGE_VULKAN_END &&
                     accumulator.last_failure_result == -4,
                 "last failure must retain its exact stage and result")) {
        return 1;
    }

    if (!require(datalab_render_perf_format_summary_json(&accumulator,
                                                         json,
                                                         sizeof(json)),
                 "summary JSON must fit the bounded output") ||
        !require(strstr(json, "\"schema\":1") &&
                     strstr(json, "\"uploads\":2") &&
                     strstr(json, "\"bytes\":34560000") &&
                     strstr(json, "\"stage\":\"vulkan_end\"") &&
                     !strchr(json, '/'),
                 "summary must expose bounded counters without filesystem paths") ||
        !require(!datalab_render_perf_format_summary_json(&accumulator, json, 16u),
                 "truncated summaries must fail closed")) {
        return 1;
    }

    puts("datalab render performance diagnostics contract test passed");
    return 0;
}
