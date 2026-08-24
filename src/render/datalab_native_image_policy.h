#ifndef DATALAB_NATIVE_IMAGE_POLICY_H
#define DATALAB_NATIVE_IMAGE_POLICY_H

#include <stddef.h>
#include <stdint.h>

typedef struct DatalabNativeImageIdentity {
    uint32_t width;
    uint32_t height;
    uint64_t content_generation;
    int sampling_mode;
    int valid;
} DatalabNativeImageIdentity;

typedef struct DatalabNativeImageFrameDecision {
    int upload_image;
    int upload_overlay;
} DatalabNativeImageFrameDecision;

int datalab_native_image_identity_valid(const DatalabNativeImageIdentity *identity);
int datalab_native_image_identity_equal(const DatalabNativeImageIdentity *a,
                                        const DatalabNativeImageIdentity *b);
int datalab_native_image_overlay_equal(const uint8_t *shadow,
                                       size_t shadow_size,
                                       uint32_t shadow_width,
                                       uint32_t shadow_height,
                                       const void *pixels,
                                       size_t row_stride_bytes,
                                       uint32_t width,
                                       uint32_t height);
int datalab_native_image_plan_frame(const DatalabNativeImageIdentity *resident,
                                    const DatalabNativeImageIdentity *requested,
                                    int overlay_equal,
                                    DatalabNativeImageFrameDecision *out_decision);

#endif
