#include "render/datalab_native_image_policy.h"

#include <string.h>

static int datalab_native_image_rgba_size(uint32_t width,
                                          uint32_t height,
                                          size_t *out_row_bytes,
                                          size_t *out_total_bytes) {
    size_t row_bytes;
    if (!out_row_bytes || !out_total_bytes || width == 0u || height == 0u ||
        (size_t)width > SIZE_MAX / 4u) {
        return 0;
    }
    row_bytes = (size_t)width * 4u;
    if ((size_t)height > SIZE_MAX / row_bytes) {
        return 0;
    }
    *out_row_bytes = row_bytes;
    *out_total_bytes = row_bytes * (size_t)height;
    return 1;
}

int datalab_native_image_identity_valid(const DatalabNativeImageIdentity *identity) {
    return identity && identity->valid && identity->width > 0u && identity->height > 0u &&
           identity->content_generation > 0u &&
           (identity->sampling_mode == 0 || identity->sampling_mode == 1);
}

int datalab_native_image_identity_equal(const DatalabNativeImageIdentity *a,
                                        const DatalabNativeImageIdentity *b) {
    return datalab_native_image_identity_valid(a) &&
           datalab_native_image_identity_valid(b) && a->width == b->width &&
           a->height == b->height && a->content_generation == b->content_generation &&
           a->sampling_mode == b->sampling_mode;
}

int datalab_native_image_overlay_equal(const uint8_t *shadow,
                                       size_t shadow_size,
                                       uint32_t shadow_width,
                                       uint32_t shadow_height,
                                       const void *pixels,
                                       size_t row_stride_bytes,
                                       uint32_t width,
                                       uint32_t height) {
    size_t row_bytes = 0u;
    size_t total_bytes = 0u;
    const uint8_t *source = (const uint8_t *)pixels;
    uint32_t row;
    if (!shadow || !pixels || shadow_width != width || shadow_height != height ||
        !datalab_native_image_rgba_size(width, height, &row_bytes, &total_bytes) ||
        shadow_size < total_bytes || row_stride_bytes < row_bytes) {
        return 0;
    }
    for (row = 0u; row < height; ++row) {
        if (memcmp(shadow + ((size_t)row * row_bytes),
                   source + ((size_t)row * row_stride_bytes),
                   row_bytes) != 0) {
            return 0;
        }
    }
    return 1;
}

int datalab_native_image_plan_frame(const DatalabNativeImageIdentity *resident,
                                    const DatalabNativeImageIdentity *requested,
                                    int overlay_equal,
                                    DatalabNativeImageFrameDecision *out_decision) {
    if (!datalab_native_image_identity_valid(requested) || !out_decision) {
        return 0;
    }
    out_decision->upload_image = !datalab_native_image_identity_equal(resident, requested);
    out_decision->upload_overlay = overlay_equal ? 0 : 1;
    return 1;
}
