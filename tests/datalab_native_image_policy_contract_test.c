#include "render/datalab_native_image_policy.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    DatalabNativeImageIdentity empty = {0};
    DatalabNativeImageIdentity resident = {2400u, 1800u, 41u, 0, 1};
    DatalabNativeImageIdentity requested = resident;
    DatalabNativeImageFrameDecision decision = {0};

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

    puts("datalab native image policy contract test passed");
    return 0;
}
