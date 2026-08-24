#include "render/datalab_native_image_policy.h"

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
