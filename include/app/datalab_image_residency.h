#ifndef DATALAB_IMAGE_RESIDENCY_H
#define DATALAB_IMAGE_RESIDENCY_H

#include <stdint.h>
#include <sys/types.h>

#include "data/pack_loader.h"
#include "app/app_state.h"

enum {
    DATALAB_IMAGE_CPU_BUDGET_BYTES = 256 * 1024 * 1024,
    DATALAB_IMAGE_GPU_BUDGET_BYTES = 256 * 1024 * 1024,
    DATALAB_IMAGE_THUMBNAIL_BUDGET_BYTES = 64 * 1024 * 1024,
    DATALAB_IMAGE_RESIDENCY_SLOT_COUNT = 4,
    DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT = 64
};

typedef struct DatalabImageIdentity {
    char canonical_path[DATALAB_APP_PATH_CAP];
    uint64_t device;
    uint64_t inode;
    uint64_t size;
    int64_t mtime_sec;
    int64_t mtime_nsec;
} DatalabImageIdentity;

typedef struct DatalabImageResidencyStats {
    uint64_t hits, misses, evictions, admission_rejections, stale_rejections;
    uint64_t resident_bytes, peak_resident_bytes;
} DatalabImageResidencyStats;

typedef struct DatalabImageResidencySlot {
    DatalabImageIdentity identity;
    DatalabFrame frame;
    uint64_t bytes;
    uint64_t lru_stamp;
    int valid;
    int pinned;
} DatalabImageResidencySlot;

typedef struct DatalabThumbnailResidencySlot {
    DatalabImageIdentity identity;
    uint8_t *rgba;
    uint32_t width;
    uint32_t height;
    uint64_t bytes;
    uint64_t lru_stamp;
    int valid;
} DatalabThumbnailResidencySlot;

typedef struct DatalabImageResidency {
    uint32_t magic;
    DatalabImageResidencySlot *cpu_slots;
    DatalabThumbnailResidencySlot thumbnail_slots[DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT];
    DatalabImageIdentity active_identity;
    uint64_t active_bytes;
    uint64_t lru_stamp;
    DatalabImageResidencyStats cpu;
    DatalabImageResidencyStats gpu;
    DatalabImageResidencyStats thumbnail;
} DatalabImageResidency;

void datalab_image_residency_init(DatalabImageResidency *residency);
void datalab_image_residency_destroy(DatalabImageResidency *residency);
int datalab_image_identity_from_path(const char *path, DatalabImageIdentity *out_identity);
int datalab_image_identity_equal(const DatalabImageIdentity *left, const DatalabImageIdentity *right);
uint64_t datalab_image_rgba_bytes(uint32_t width, uint32_t height);
void datalab_image_residency_note_active(DatalabImageResidency *residency,
                                         const char *path,
                                         const DatalabFrame *frame);
int datalab_image_residency_can_activate(const DatalabImageResidency *residency,
                                         const DatalabImageIdentity *identity,
                                         const DatalabFrame *frame);
void datalab_image_residency_clear_cpu(DatalabImageResidency *residency);
int datalab_image_residency_store_cpu(DatalabImageResidency *residency,
                                      const char *path,
                                      DatalabFrame *io_frame);
int datalab_image_residency_store_cpu_identity(DatalabImageResidency *residency,
                                               const DatalabImageIdentity *identity,
                                               DatalabFrame *io_frame);
int datalab_image_residency_take_cpu(DatalabImageResidency *residency,
                                     const char *path,
                                     DatalabFrame *out_frame);
int datalab_image_residency_identity_is_current(const DatalabImageIdentity *identity);
/*
 * Transfers rgba ownership to the cache only when it returns 1. On failure,
 * the caller still owns rgba and every existing cache slot is unchanged.
 */
int datalab_image_residency_admit_thumbnail_pixels(DatalabImageResidency *residency,
                                                   const DatalabImageIdentity *identity,
                                                   uint8_t *rgba,
                                                   uint32_t width,
                                                   uint32_t height);
const DatalabThumbnailResidencySlot *datalab_image_residency_find_thumbnail(DatalabImageResidency *residency,
                                                                             const DatalabImageIdentity *identity);
void datalab_image_residency_note_gpu(DatalabImageResidency *residency,
                                      uint64_t bytes, int hit);
void datalab_image_residency_note_thumbnail(DatalabImageResidency *residency,
                                            uint64_t bytes, int hit);

#endif
