#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/datalab_image_residency.h"
#include "core_base.h"
#include "render/render_view_library_preview.h"

enum { THUMBNAIL_FIXTURE_COUNT = 1000 };

static int require(int condition, const char *message) {
    if (!condition) fprintf(stderr, "image-residency-contract: %s\n", message);
    return condition;
}

static int count_valid_thumbnails(const DatalabImageResidency *residency) {
    int count = 0;
    for (int i = 0; residency && i < DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT; ++i) {
        if (residency->thumbnail_slots[i].valid) ++count;
    }
    return count;
}

static uint8_t *make_thumbnail_pixels(uint32_t width, uint32_t height, uint8_t fill) {
    uint64_t bytes = datalab_image_rgba_bytes(width, height);
    uint8_t *rgba = bytes == 0u ? NULL : (uint8_t *)core_alloc((size_t)bytes);
    if (rgba) memset(rgba, fill, (size_t)bytes);
    return rgba;
}

static int make_identity(const char *directory, int index, DatalabImageIdentity *out_identity) {
    char path[256];
    int fd = 0;
    if (!directory || !out_identity || snprintf(path, sizeof(path), "%s/thumbnail-%04d.png", directory, index) >= (int)sizeof(path)) return 0;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return 0;
    if (write(fd, "t", 1) != 1 || close(fd) != 0) return 0;
    return datalab_image_identity_from_path(path, out_identity);
}

static void remove_identities(const char *directory) {
    char path[256];
    if (!directory) return;
    for (int i = 0; i < THUMBNAIL_FIXTURE_COUNT; ++i) {
        if (snprintf(path, sizeof(path), "%s/thumbnail-%04d.png", directory, i) < (int)sizeof(path)) unlink(path);
    }
    rmdir(directory);
}

int main(void) {
    DatalabImageResidency residency;
    DatalabImageIdentity identities[THUMBNAIL_FIXTURE_COUNT];
    const DatalabThumbnailResidencySlot *thumbnail = NULL;
    uint8_t *rgba = NULL;
    uint32_t thumbnail_width = 0u;
    uint32_t thumbnail_height = 0u;
    char directory[] = "/private/tmp/datalab-thumbnail-residency.XXXXXX";

    if (!require(datalab_image_rgba_bytes(3840u, 2160u) == 33177600u, "4K RGBA byte math must be exact") ||
        !require(datalab_library_preview_thumbnail_dimensions(4096u, 2048u, &thumbnail_width, &thumbnail_height) &&
                     thumbnail_width == 512u && thumbnail_height == 256u,
                 "thumbnail must preserve aspect ratio with a 512px maximum edge") ||
        !require(!datalab_library_preview_debounce_ready(100u, 139u) &&
                     datalab_library_preview_debounce_ready(100u, 140u),
                 "thumbnail selection must use a responsive 40ms debounce") ||
        !require(mkdtemp(directory) != NULL, "thumbnail fixture directory creation failed")) return 1;
    for (int i = 0; i < THUMBNAIL_FIXTURE_COUNT; ++i) {
        if (!require(make_identity(directory, i, &identities[i]), "thumbnail identity creation failed")) {
            remove_identities(directory);
            return 1;
        }
    }

    datalab_image_residency_init(&residency);
    if (!require(count_valid_thumbnails(&residency) == 0 && residency.thumbnail.resident_bytes == 0u,
                 "zero-thumbnail cache must begin empty")) goto fail;

    rgba = make_thumbnail_pixels(2u, 2u, 0x11);
    if (!require(rgba != NULL, "one-thumbnail allocation failed") ||
        !require(datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[0], rgba, 2u, 2u),
                 "one-thumbnail atomic admission failed") ||
        !require((thumbnail = datalab_image_residency_find_thumbnail(&residency, &identities[0])) != NULL && thumbnail->rgba == rgba &&
                     count_valid_thumbnails(&residency) == 1,
                 "successful admission must transfer caller buffer ownership exactly once")) goto fail;

    rgba = make_thumbnail_pixels(2u, 2u, 0x22);
    if (!require(rgba != NULL, "same-identity replacement allocation failed") ||
        !require(datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[0], rgba, 2u, 2u),
                 "same-identity replacement failed") ||
        !require((thumbnail = datalab_image_residency_find_thumbnail(&residency, &identities[0])) != NULL && thumbnail->rgba == rgba &&
                     residency.thumbnail.resident_bytes == 16u,
                 "same-identity replacement must retain only the replacement buffer")) goto fail;

    for (int i = 1; i < DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT; ++i) {
        rgba = make_thumbnail_pixels(2u, 2u, (uint8_t)i);
        if (!require(rgba != NULL && datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[i], rgba, 2u, 2u),
                     "64-thumbnail admission failed")) goto fail;
    }
    if (!require(count_valid_thumbnails(&residency) == 64, "64 unique thumbnails must fill all slots")) goto fail;
    (void)datalab_image_residency_find_thumbnail(&residency, &identities[0]);
    rgba = make_thumbnail_pixels(2u, 2u, 0x65);
    if (!require(rgba != NULL && datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[64], rgba, 2u, 2u),
                 "65th thumbnail admission failed") ||
        !require(count_valid_thumbnails(&residency) == 64 &&
                     datalab_image_residency_find_thumbnail(&residency, &identities[0]) != NULL &&
                     datalab_image_residency_find_thumbnail(&residency, &identities[1]) == NULL,
                 "65th thumbnail must replace the LRU slot without a stale owner")) goto fail;

    for (int i = 65; i < THUMBNAIL_FIXTURE_COUNT; ++i) {
        rgba = make_thumbnail_pixels(2u, 2u, (uint8_t)i);
        if (!require(rgba != NULL && datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[i], rgba, 2u, 2u),
                     "repeated thumbnail turnover failed")) goto fail;
    }
    if (!require(count_valid_thumbnails(&residency) == 64 && residency.thumbnail.resident_bytes == 64u * 16u,
                 "1000 unique thumbnails must retain one bounded, exact 64-slot cache")) goto fail;
    datalab_image_residency_destroy(&residency);

    datalab_image_residency_init(&residency);
    rgba = make_thumbnail_pixels(8192u, 1280u, 0x33);
    if (!require(rgba != NULL && datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[0], rgba, 8192u, 1280u),
                 "byte-budget first admission failed")) goto fail;
    rgba = make_thumbnail_pixels(8192u, 1280u, 0x44);
    if (!require(rgba != NULL && datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[1], rgba, 8192u, 1280u),
                 "byte-budget replacement admission failed") ||
        !require(datalab_image_residency_find_thumbnail(&residency, &identities[0]) == NULL &&
                     datalab_image_residency_find_thumbnail(&residency, &identities[1]) != NULL &&
                     residency.thumbnail.resident_bytes == 8192u * 1280u * 4u,
                 "byte budget must evict a complete previous slot before transfer")) goto fail;
    thumbnail = datalab_image_residency_find_thumbnail(&residency, &identities[1]);
    rgba = (uint8_t *)core_alloc(1u);
    if (!require(rgba != NULL && !datalab_image_residency_admit_thumbnail_pixels(&residency, &identities[2], rgba, 8192u, 2049u),
                 "oversized thumbnail must fail admission") ||
        !require(datalab_image_residency_find_thumbnail(&residency, &identities[1]) == thumbnail &&
                     thumbnail->rgba != NULL && residency.thumbnail.resident_bytes == 8192u * 1280u * 4u,
                 "failed admission must preserve valid cache state and caller ownership")) goto fail;
    core_free(rgba);
    datalab_image_residency_destroy(&residency);
    remove_identities(directory);
    puts("datalab image residency contract test passed");
    return 0;

fail:
    datalab_image_residency_destroy(&residency);
    remove_identities(directory);
    return 1;
}
