#include "render/datalab_image_overlay_identity.h"

#include <stddef.h>
#include <string.h>

#include "app/app_state.h"
#include "data/pack_loader.h"

typedef struct DatalabOverlayDigest {
    uint64_t low;
    uint64_t high;
} DatalabOverlayDigest;

static void datalab_overlay_digest_bytes(DatalabOverlayDigest *digest,
                                         const void *bytes,
                                         size_t size) {
    const uint8_t *cursor = (const uint8_t *)bytes;
    size_t i;
    if (!digest || (!bytes && size > 0u)) {
        return;
    }
    for (i = 0u; i < size; ++i) {
        digest->low ^= cursor[i];
        digest->low *= UINT64_C(1099511628211);
        digest->high ^= (uint64_t)(cursor[i] + 0x9du);
        digest->high *= UINT64_C(14029467366897019727);
    }
}

static void datalab_overlay_digest_u64(DatalabOverlayDigest *digest, uint64_t value) {
    datalab_overlay_digest_bytes(digest, &value, sizeof(value));
}

static void datalab_overlay_digest_string(DatalabOverlayDigest *digest,
                                          const char *value) {
    const size_t length = value ? strlen(value) : 0u;
    datalab_overlay_digest_u64(digest, (uint64_t)length);
    datalab_overlay_digest_bytes(digest, value, length);
}

int datalab_image_overlay_identity_equal(const DatalabImageOverlayIdentity *a,
                                         const DatalabImageOverlayIdentity *b) {
    return a && b && a->valid && b->valid &&
           a->digest_low == b->digest_low &&
           a->digest_high == b->digest_high &&
           a->drawable_width == b->drawable_width &&
           a->drawable_height == b->drawable_height;
}

int datalab_image_overlay_identity_build(const DatalabFrame *frame,
                                         const DatalabAppState *app_state,
                                         uint32_t drawable_width,
                                         uint32_t drawable_height,
                                         uint64_t session_visual_revision,
                                         DatalabImageOverlayIdentity *out_identity) {
    DatalabOverlayDigest digest = {
        UINT64_C(1469598103934665603),
        UINT64_C(11400714785074694791)
    };
    size_t recent_count;
    size_t i;
    if (!frame || !app_state || !out_identity || frame->profile != DATALAB_PROFILE_IMAGE ||
        !frame->drawing_rgba || drawable_width == 0u || drawable_height == 0u) {
        return 0;
    }
    recent_count = app_state->recent_input_root_count;
    if (recent_count > DATALAB_RECENT_INPUT_ROOT_LIMIT) {
        recent_count = DATALAB_RECENT_INPUT_ROOT_LIMIT;
    }
    datalab_overlay_digest_u64(&digest, frame->raster_content_generation);
    datalab_overlay_digest_u64(&digest, (uint64_t)frame->image_metadata.format);
    datalab_overlay_digest_u64(&digest, (uint64_t)frame->image_metadata.transfer);
    datalab_overlay_digest_u64(&digest, frame->image_metadata.source_bit_depth);
    datalab_overlay_digest_u64(&digest, frame->image_metadata.source_has_alpha);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->profile);
    datalab_overlay_digest_u64(&digest, (uint64_t)(uint32_t)app_state->text_zoom_step);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->panel_selected_index);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->session_hud_collapsed);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->sampling_mode);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->raster_actual_pixel_mode);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->raster_probe_valid);
    datalab_overlay_digest_u64(&digest, app_state->raster_probe_x);
    datalab_overlay_digest_u64(&digest, app_state->raster_probe_y);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->playback_active);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->playback_mode);
    datalab_overlay_digest_u64(&digest, (uint64_t)(uint32_t)app_state->playback_speed_index);
    datalab_overlay_digest_u64(&digest, (uint64_t)app_state->recent_input_root_dropdown_open);
    datalab_overlay_digest_u64(&digest, app_state->workspace_authoring_theme_preset_id);
    datalab_overlay_digest_bytes(&digest,
                                 &app_state->workspace_authoring_custom_theme,
                                 sizeof(app_state->workspace_authoring_custom_theme));
    datalab_overlay_digest_string(&digest, app_state->pack_path);
    datalab_overlay_digest_string(&digest, app_state->input_root);
    datalab_overlay_digest_u64(&digest, (uint64_t)recent_count);
    for (i = 0u; i < recent_count; ++i) {
        datalab_overlay_digest_string(&digest, app_state->recent_input_roots[i]);
    }
    if (app_state->raster_probe_valid &&
        app_state->raster_probe_x < frame->width &&
        app_state->raster_probe_y < frame->height) {
        const size_t pixel_offset =
            (((size_t)app_state->raster_probe_y * frame->width) +
             app_state->raster_probe_x) * 4u;
        datalab_overlay_digest_bytes(&digest, frame->drawing_rgba + pixel_offset, 4u);
    }
    datalab_overlay_digest_u64(&digest, session_visual_revision);
    *out_identity = (DatalabImageOverlayIdentity){
        digest.low,
        digest.high,
        drawable_width,
        drawable_height,
        1
    };
    return 1;
}
