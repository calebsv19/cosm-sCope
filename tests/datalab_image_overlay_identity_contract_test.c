#include "render/datalab_image_overlay_identity.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/app_state.h"
#include "data/pack_loader.h"

int main(void) {
    uint8_t pixel[4] = {10u, 20u, 30u, 255u};
    DatalabFrame frame = {0};
    DatalabAppState app_state = {0};
    DatalabImageOverlayIdentity baseline = {0};
    DatalabImageOverlayIdentity candidate = {0};

    frame.profile = DATALAB_PROFILE_IMAGE;
    frame.width = 1u;
    frame.height = 1u;
    frame.drawing_rgba = pixel;
    frame.raster_content_generation = 7u;
    frame.image_metadata.format = DATALAB_IMAGE_FORMAT_PNG;
    frame.image_metadata.transfer = DATALAB_IMAGE_TRANSFER_SRGB;
    frame.image_metadata.source_bit_depth = 8u;
    frame.image_metadata.source_has_alpha = 1u;
    app_state.profile = DATALAB_PROFILE_IMAGE;
    app_state.sampling_mode = DATALAB_SAMPLING_MODE_LINEAR;
    snprintf(app_state.input_root, sizeof(app_state.input_root), "%s", "/images");

    assert(datalab_image_overlay_identity_build(&frame,
                                                &app_state,
                                                1200u,
                                                900u,
                                                11u,
                                                &baseline));
    assert(baseline.valid);

    /* View geometry is deliberately excluded: zoom and pan do not change any
     * overlay pixels. */
    app_state.raster_viewport.viewport.zoom = 3.0;
    app_state.raster_viewport.viewport.pan_x = 123.0;
    app_state.raster_viewport.viewport.pan_y = -45.0;
    assert(datalab_image_overlay_identity_build(&frame,
                                                &app_state,
                                                1200u,
                                                900u,
                                                11u,
                                                &candidate));
    assert(datalab_image_overlay_identity_equal(&baseline, &candidate));

    app_state.sampling_mode = DATALAB_SAMPLING_MODE_NEAREST;
    assert(datalab_image_overlay_identity_build(&frame, &app_state, 1200u, 900u, 11u,
                                                &candidate));
    assert(!datalab_image_overlay_identity_equal(&baseline, &candidate));

    app_state.sampling_mode = DATALAB_SAMPLING_MODE_LINEAR;
    app_state.raster_probe_valid = 1;
    assert(datalab_image_overlay_identity_build(&frame, &app_state, 1200u, 900u, 11u,
                                                &candidate));
    assert(!datalab_image_overlay_identity_equal(&baseline, &candidate));

    app_state.raster_probe_valid = 0;
    assert(datalab_image_overlay_identity_build(&frame, &app_state, 1201u, 900u, 11u,
                                                &candidate));
    assert(!datalab_image_overlay_identity_equal(&baseline, &candidate));
    assert(datalab_image_overlay_identity_build(&frame, &app_state, 1200u, 900u, 12u,
                                                &candidate));
    assert(!datalab_image_overlay_identity_equal(&baseline, &candidate));

    puts("datalab image overlay identity contract test passed");
    return 0;
}
