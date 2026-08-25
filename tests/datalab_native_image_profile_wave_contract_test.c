#include <stdio.h>
#include <string.h>

#include "render/datalab_native_image_profile_wave.h"

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "native-image-profile-wave-contract: %s\n", message);
    }
    return condition;
}

int main(void) {
    DatalabNativeImageProfileDelta delta;

    memset(&delta, 0, sizeof(delta));
    delta.counters.image_reuse_count = 1u;
    delta.counters.overlay_reuse_count = 1u;
    if (!require(datalab_native_image_profile_stage_passes(
                     DATALAB_NATIVE_IMAGE_PROFILE_ZOOM_BURST, &delta),
                 "stable zoom must accept reuse without uploads") ||
        !require(datalab_native_image_profile_stage_passes(
                     DATALAB_NATIVE_IMAGE_PROFILE_PAN, &delta),
                 "stable pan must accept reuse without uploads")) {
        return 1;
    }

    delta.counters.overlay_upload_count = 1u;
    if (!require(!datalab_native_image_profile_stage_passes(
                     DATALAB_NATIVE_IMAGE_PROFILE_ZOOM_BURST, &delta),
                 "stable zoom must reject compatibility overlay uploads")) {
        return 1;
    }

    memset(&delta, 0, sizeof(delta));
    delta.counters.image_reuse_count = 1u;
    delta.counters.overlay_upload_count = 1u;
    delta.counters.overlay_redraw_count = 1u;
    if (!require(datalab_native_image_profile_stage_passes(
                     DATALAB_NATIVE_IMAGE_PROFILE_HUD_TOGGLE, &delta),
                 "HUD mutation must redraw exactly one overlay")) {
        return 1;
    }

    memset(&delta, 0, sizeof(delta));
    delta.counters.image_reuse_count = 1u;
    delta.counters.presentation_recreate_count = 1u;
    delta.counters.presentation_seed_upload_count = 1u;
    delta.counters.presentation_seed_upload_bytes = 4096u;
    if (!require(datalab_native_image_profile_stage_passes(
                     DATALAB_NATIVE_IMAGE_PROFILE_RESIZE, &delta),
                 "resize must expose presentation recreation separately")) {
        return 1;
    }

    memset(&delta, 0, sizeof(delta));
    delta.counters.image_upload_count = 1u;
    if (!require(datalab_native_image_profile_stage_passes(
                     DATALAB_NATIVE_IMAGE_PROFILE_CONTENT_REPLACE, &delta),
                 "content generation replacement must upload exactly once") ||
        !require(!datalab_native_image_profile_stage_passes(
                     DATALAB_NATIVE_IMAGE_PROFILE_CONTENT_REPLACE,
                     &(DatalabNativeImageProfileDelta){0}),
                 "content replacement must fail closed without an upload")) {
        return 1;
    }

    puts("datalab native image profile wave contract test passed");
    return 0;
}
