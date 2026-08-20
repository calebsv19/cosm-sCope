#include "app/datalab_image_residency.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DATALAB_IMAGE_RESIDENCY_MAGIC 0x444c4934u

static void datalab_image_residency_update_peak(DatalabImageResidencyStats *stats) {
    if (stats && stats->resident_bytes > stats->peak_resident_bytes) stats->peak_resident_bytes = stats->resident_bytes;
}

uint64_t datalab_image_rgba_bytes(uint32_t width, uint32_t height) {
    if (width == 0u || height == 0u || width > UINT64_MAX / height / 4u) return 0u;
    return (uint64_t)width * (uint64_t)height * 4u;
}

int datalab_image_identity_from_path(const char *path, DatalabImageIdentity *out_identity) {
    struct stat info;
    char resolved[PATH_MAX];
    if (!path || !out_identity || !realpath(path, resolved) || stat(resolved, &info) != 0) return 0;
    memset(out_identity, 0, sizeof(*out_identity));
    (void)snprintf(out_identity->canonical_path, sizeof(out_identity->canonical_path), "%s", resolved);
    out_identity->device = (uint64_t)info.st_dev;
    out_identity->inode = (uint64_t)info.st_ino;
    out_identity->size = (uint64_t)info.st_size;
#if defined(__APPLE__)
    out_identity->mtime_sec = info.st_mtimespec.tv_sec;
    out_identity->mtime_nsec = info.st_mtimespec.tv_nsec;
#else
    out_identity->mtime_sec = info.st_mtim.tv_sec;
    out_identity->mtime_nsec = info.st_mtim.tv_nsec;
#endif
    return 1;
}

int datalab_image_identity_equal(const DatalabImageIdentity *left, const DatalabImageIdentity *right) {
    return left && right && left->canonical_path[0] && right->canonical_path[0] &&
           strcmp(left->canonical_path, right->canonical_path) == 0 && left->device == right->device &&
           left->inode == right->inode && left->size == right->size && left->mtime_sec == right->mtime_sec &&
           left->mtime_nsec == right->mtime_nsec;
}

void datalab_image_residency_init(DatalabImageResidency *residency) {
    if (!residency) return;
    memset(residency, 0, sizeof(*residency));
    residency->magic = DATALAB_IMAGE_RESIDENCY_MAGIC;
    residency->cpu_slots = (DatalabImageResidencySlot *)core_alloc(sizeof(*residency->cpu_slots) * DATALAB_IMAGE_RESIDENCY_SLOT_COUNT);
    if (!residency->cpu_slots) return;
    memset(residency->cpu_slots, 0, sizeof(*residency->cpu_slots) * DATALAB_IMAGE_RESIDENCY_SLOT_COUNT);
    for (int i = 0; i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) datalab_frame_init(&residency->cpu_slots[i].frame);
}

void datalab_image_residency_destroy(DatalabImageResidency *residency) {
    if (!residency || residency->magic != DATALAB_IMAGE_RESIDENCY_MAGIC) return;
    for (int i = 0; residency->cpu_slots && i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) datalab_frame_free(&residency->cpu_slots[i].frame);
    for (int i = 0; i < DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT; ++i) {
        core_free(residency->thumbnail_slots[i].rgba);
        memset(&residency->thumbnail_slots[i], 0, sizeof(residency->thumbnail_slots[i]));
    }
    core_free(residency->cpu_slots);
    memset(residency, 0, sizeof(*residency));
}

void datalab_image_residency_clear_cpu(DatalabImageResidency *residency) {
    if (!residency || residency->magic != DATALAB_IMAGE_RESIDENCY_MAGIC) return;
    for (int i = 0; residency->cpu_slots && i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) {
        DatalabImageResidencySlot *slot = &residency->cpu_slots[i];
        if (slot->valid) datalab_frame_free(&slot->frame);
        memset(slot, 0, sizeof(*slot));
        datalab_frame_init(&slot->frame);
    }
    residency->cpu.resident_bytes = 0u;
}

void datalab_image_residency_note_active(DatalabImageResidency *residency, const char *path, const DatalabFrame *frame) {
    if (!residency || !frame || frame->profile != DATALAB_PROFILE_IMAGE) return;
    if (!datalab_image_identity_from_path(path, &residency->active_identity)) return;
    residency->active_bytes = datalab_image_rgba_bytes(frame->width, frame->height);
}

int datalab_image_residency_can_activate(const DatalabImageResidency *residency,
                                         const DatalabImageIdentity *identity,
                                         const DatalabFrame *frame) {
    uint64_t bytes = 0u;
    if (!residency || !identity || !frame || frame->profile != DATALAB_PROFILE_IMAGE || !frame->drawing_rgba ||
        !datalab_image_residency_identity_is_current(identity)) return 0;
    bytes = datalab_image_rgba_bytes(frame->width, frame->height);
    return bytes != 0u && bytes <= DATALAB_IMAGE_CPU_BUDGET_BYTES;
}

static int datalab_image_residency_evict_one(DatalabImageResidency *residency) {
    DatalabImageResidencySlot *oldest = NULL;
    for (int i = 0; residency && residency->cpu_slots && i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) {
        DatalabImageResidencySlot *slot = &residency->cpu_slots[i];
        if (slot->valid && !slot->pinned && (!oldest || slot->lru_stamp < oldest->lru_stamp)) oldest = slot;
    }
    if (!oldest) return 0;
    residency->cpu.resident_bytes -= oldest->bytes;
    residency->cpu.evictions++;
    datalab_frame_free(&oldest->frame);
    datalab_frame_init(&oldest->frame);
    memset(oldest, 0, sizeof(*oldest));
    return 1;
}

int datalab_image_residency_store_cpu_identity(DatalabImageResidency *residency,
                                               const DatalabImageIdentity *identity,
                                               DatalabFrame *io_frame) {
    DatalabImageResidencySlot *slot = NULL;
    uint64_t bytes = 0u;
    if (!residency || !residency->cpu_slots || !io_frame || io_frame->profile != DATALAB_PROFILE_IMAGE || !io_frame->drawing_rgba ||
        !identity) return 0;
    if (!datalab_image_residency_identity_is_current(identity)) {
        residency->cpu.stale_rejections++;
        return 0;
    }
    bytes = datalab_image_rgba_bytes(io_frame->width, io_frame->height);
    if (bytes == 0u || !datalab_image_residency_identity_is_current(identity)) {
        residency->cpu.stale_rejections++;
        return 0;
    }
    for (int i = 0; i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) {
        if (residency->cpu_slots[i].valid && datalab_image_identity_equal(&residency->cpu_slots[i].identity, identity)) {
            residency->cpu_slots[i].lru_stamp = ++residency->lru_stamp;
            residency->cpu.hits++;
            return 0;
        }
    }
    while (residency->active_bytes + residency->cpu.resident_bytes + bytes > DATALAB_IMAGE_CPU_BUDGET_BYTES &&
           datalab_image_residency_evict_one(residency)) {}
    if (residency->active_bytes + residency->cpu.resident_bytes + bytes > DATALAB_IMAGE_CPU_BUDGET_BYTES) {
        residency->cpu.admission_rejections++;
        return 0;
    }
    for (int i = 0; i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) if (!residency->cpu_slots[i].valid) { slot = &residency->cpu_slots[i]; break; }
    if (!slot && datalab_image_residency_evict_one(residency)) {
        for (int i = 0; i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) if (!residency->cpu_slots[i].valid) { slot = &residency->cpu_slots[i]; break; }
    }
    if (!slot) { residency->cpu.admission_rejections++; return 0; }
    slot->identity = *identity;
    slot->frame = *io_frame;
    datalab_frame_init(io_frame);
    slot->bytes = bytes;
    slot->lru_stamp = ++residency->lru_stamp;
    slot->valid = 1;
    residency->cpu.misses++;
    residency->cpu.resident_bytes += bytes;
    datalab_image_residency_update_peak(&residency->cpu);
    return 1;
}

int datalab_image_residency_store_cpu(DatalabImageResidency *residency, const char *path, DatalabFrame *io_frame) {
    DatalabImageIdentity identity;
    if (!datalab_image_identity_from_path(path, &identity)) return 0;
    return datalab_image_residency_store_cpu_identity(residency, &identity, io_frame);
}

int datalab_image_residency_take_cpu(DatalabImageResidency *residency, const char *path, DatalabFrame *out_frame) {
    DatalabImageIdentity identity;
    if (!residency || !residency->cpu_slots || !out_frame || !datalab_image_identity_from_path(path, &identity)) return 0;
    for (int i = 0; i < DATALAB_IMAGE_RESIDENCY_SLOT_COUNT; ++i) {
        DatalabImageResidencySlot *slot = &residency->cpu_slots[i];
        if (slot->valid && datalab_image_identity_equal(&slot->identity, &identity)) {
            *out_frame = slot->frame; datalab_frame_init(&slot->frame); residency->cpu.resident_bytes -= slot->bytes;
            memset(slot, 0, sizeof(*slot)); residency->cpu.hits++; return 1;
        }
    }
    residency->cpu.misses++; return 0;
}

int datalab_image_residency_identity_is_current(const DatalabImageIdentity *identity) {
    DatalabImageIdentity now;
    return identity && datalab_image_identity_from_path(identity->canonical_path, &now) && datalab_image_identity_equal(identity, &now);
}

static void datalab_image_residency_clear_thumbnail_slot(DatalabImageResidency *residency,
                                                         DatalabThumbnailResidencySlot *slot) {
    if (!residency || !slot || !slot->valid) return;
    residency->thumbnail.resident_bytes -= slot->bytes;
    residency->thumbnail.evictions++;
    core_free(slot->rgba);
    memset(slot, 0, sizeof(*slot));
}

static DatalabThumbnailResidencySlot *datalab_image_residency_oldest_thumbnail_slot(DatalabImageResidency *residency,
                                                                                      const DatalabThumbnailResidencySlot *exclude) {
    DatalabThumbnailResidencySlot *oldest = NULL;
    for (int i = 0; residency && i < DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT; ++i) {
        DatalabThumbnailResidencySlot *candidate = &residency->thumbnail_slots[i];
        if (candidate != exclude && candidate->valid && (!oldest || candidate->lru_stamp < oldest->lru_stamp)) oldest = candidate;
    }
    return oldest;
}

int datalab_image_residency_admit_thumbnail_pixels(DatalabImageResidency *residency,
                                                   const DatalabImageIdentity *identity,
                                                   uint8_t *rgba,
                                                   uint32_t width,
                                                   uint32_t height) {
    uint64_t bytes = datalab_image_rgba_bytes(width, height);
    DatalabThumbnailResidencySlot *slot = NULL;
    if (!residency || !identity || !rgba || bytes == 0u || !datalab_image_residency_identity_is_current(identity)) return 0;
    if (bytes > DATALAB_IMAGE_THUMBNAIL_BUDGET_BYTES) {
        residency->thumbnail.admission_rejections++;
        return 0;
    }
    for (int i = 0; i < DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT; ++i) {
        DatalabThumbnailResidencySlot *candidate = &residency->thumbnail_slots[i];
        if (candidate->valid && datalab_image_identity_equal(&candidate->identity, identity)) {
            slot = candidate;
            break;
        }
    }
    while (residency->thumbnail.resident_bytes + bytes - (slot ? slot->bytes : 0u) > DATALAB_IMAGE_THUMBNAIL_BUDGET_BYTES) {
        DatalabThumbnailResidencySlot *oldest = datalab_image_residency_oldest_thumbnail_slot(residency, slot);
        if (!oldest) {
            residency->thumbnail.admission_rejections++;
            return 0;
        }
        datalab_image_residency_clear_thumbnail_slot(residency, oldest);
    }
    if (!slot) {
        for (int i = 0; i < DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT; ++i) {
            if (!residency->thumbnail_slots[i].valid) {
                slot = &residency->thumbnail_slots[i];
                break;
            }
        }
    }
    if (!slot) {
        slot = datalab_image_residency_oldest_thumbnail_slot(residency, NULL);
        if (!slot) {
            residency->thumbnail.admission_rejections++;
            return 0;
        }
    }
    if (slot->valid) datalab_image_residency_clear_thumbnail_slot(residency, slot);
    slot->identity = *identity;
    slot->rgba = rgba;
    slot->width = width;
    slot->height = height;
    slot->bytes = bytes;
    slot->lru_stamp = ++residency->lru_stamp;
    slot->valid = 1;
    residency->thumbnail.misses++;
    residency->thumbnail.resident_bytes += bytes;
    datalab_image_residency_update_peak(&residency->thumbnail);
    return 1;
}

const DatalabThumbnailResidencySlot *datalab_image_residency_find_thumbnail(DatalabImageResidency *residency,
                                                                             const DatalabImageIdentity *identity) {
    if (!residency || !identity || !datalab_image_residency_identity_is_current(identity)) return NULL;
    for (int i = 0; i < DATALAB_IMAGE_THUMBNAIL_SLOT_COUNT; ++i) {
        DatalabThumbnailResidencySlot *slot = &residency->thumbnail_slots[i];
        if (slot->valid && slot->rgba && datalab_image_identity_equal(&slot->identity, identity)) {
            slot->lru_stamp = ++residency->lru_stamp;
            residency->thumbnail.hits++;
            return slot;
        }
    }
    residency->thumbnail.misses++;
    return NULL;
}

void datalab_image_residency_note_gpu(DatalabImageResidency *residency, uint64_t bytes, int hit) {
    if (!residency) return;
    if (hit) residency->gpu.hits++; else residency->gpu.misses++;
    residency->gpu.resident_bytes = bytes;
    datalab_image_residency_update_peak(&residency->gpu);
}

void datalab_image_residency_note_thumbnail(DatalabImageResidency *residency, uint64_t bytes, int hit) {
    if (!residency) return;
    if (hit) residency->thumbnail.hits++; else residency->thumbnail.misses++;
    residency->thumbnail.resident_bytes = bytes > DATALAB_IMAGE_THUMBNAIL_BUDGET_BYTES ? 0u : bytes;
    if (bytes > DATALAB_IMAGE_THUMBNAIL_BUDGET_BYTES) residency->thumbnail.admission_rejections++;
    datalab_image_residency_update_peak(&residency->thumbnail);
}
