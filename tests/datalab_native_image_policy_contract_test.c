#include "render/datalab_native_image_policy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    DatalabNativeImageIdentity empty = {0};
    DatalabNativeImageIdentity resident = {2400u, 1800u, 41u, 0, 1};
    DatalabNativeImageIdentity requested = resident;
    DatalabNativeImageFrameDecision decision = {0};
    uint8_t shadow[16] = {
        1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
        9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u
    };
    uint8_t padded_rows[24] = {
        1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 99u, 99u, 99u, 99u,
        9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 99u, 99u, 99u, 99u
    };

    assert(!datalab_native_image_identity_valid(&empty));
    assert(datalab_native_image_identity_valid(&resident));

    assert(datalab_native_image_plan_frame(&empty, &requested, 0, &decision));
    assert(decision.upload_image == 1);
    assert(decision.upload_overlay == 1);

    /* Destination geometry is deliberately outside texture identity. Stable
     * zoom and pan therefore submit new draw geometry with zero byte uploads. */
    assert(datalab_native_image_plan_frame(&resident, &requested, 1, &decision));
    assert(decision.upload_image == 0);
    assert(decision.upload_overlay == 0);

    requested.content_generation += 1u;
    assert(datalab_native_image_plan_frame(&resident, &requested, 1, &decision));
    assert(decision.upload_image == 1);
    assert(decision.upload_overlay == 0);

    requested = resident;
    requested.sampling_mode = 1;
    assert(datalab_native_image_plan_frame(&resident, &requested, 1, &decision));
    assert(decision.upload_image == 1);

    assert(datalab_native_image_overlay_equal(shadow,
                                              sizeof(shadow),
                                              2u,
                                              2u,
                                              padded_rows,
                                              12u,
                                              2u,
                                              2u));
    padded_rows[13] ^= 0xffu;
    assert(!datalab_native_image_overlay_equal(shadow,
                                               sizeof(shadow),
                                               2u,
                                               2u,
                                               padded_rows,
                                               12u,
                                               2u,
                                               2u));
    assert(!datalab_native_image_overlay_equal(shadow,
                                               sizeof(shadow),
                                               2u,
                                               2u,
                                               shadow,
                                               8u,
                                               1u,
                                               2u));

    puts("datalab native image policy contract test passed");
    return 0;
}
