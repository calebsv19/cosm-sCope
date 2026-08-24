#ifndef DATALAB_IMAGE_OVERLAY_IDENTITY_H
#define DATALAB_IMAGE_OVERLAY_IDENTITY_H

#include <stdint.h>

typedef struct DatalabFrame DatalabFrame;
typedef struct DatalabAppState DatalabAppState;

typedef struct DatalabImageOverlayIdentity {
    uint64_t digest_low;
    uint64_t digest_high;
    uint32_t drawable_width;
    uint32_t drawable_height;
    int valid;
} DatalabImageOverlayIdentity;

int datalab_image_overlay_identity_equal(const DatalabImageOverlayIdentity *a,
                                         const DatalabImageOverlayIdentity *b);
int datalab_image_overlay_identity_build(const DatalabFrame *frame,
                                         const DatalabAppState *app_state,
                                         uint32_t drawable_width,
                                         uint32_t drawable_height,
                                         uint64_t session_visual_revision,
                                         DatalabImageOverlayIdentity *out_identity);

#endif
