#ifndef DATALAB_THUMBNAIL_DECODE_H
#define DATALAB_THUMBNAIL_DECODE_H

#include <stdint.h>

#include "app/datalab_image_residency.h"
#include "core_base.h"

typedef struct DatalabThumbnailDecode DatalabThumbnailDecode;

typedef struct DatalabThumbnailDecodeCompletion {
    uint64_t generation;
    DatalabImageIdentity identity;
    uint8_t *rgba;
    uint32_t width;
    uint32_t height;
    CoreResult result;
} DatalabThumbnailDecodeCompletion;

typedef struct DatalabThumbnailDecodeStats {
    uint64_t selection_count;
    uint64_t submitted_count;
    uint64_t completed_count;
    uint64_t stale_discard_count;
    uint64_t deduplicated_count;
    uint64_t rejected_count;
    uint64_t peak_outstanding;
} DatalabThumbnailDecodeStats;

DatalabThumbnailDecode *datalab_thumbnail_decode_create(void);
void datalab_thumbnail_decode_cancel(DatalabThumbnailDecode *decode);
void datalab_thumbnail_decode_destroy(DatalabThumbnailDecode *decode);

/* Selects the latest desired image and schedules at most one decode at a time.
 * A busy worker is never waited on; callers can retry the same request on the
 * next UI frame and the newest generation will run when capacity is free. */
int datalab_thumbnail_decode_request(DatalabThumbnailDecode *decode,
                                     const char *path,
                                     uint32_t max_edge,
                                     uint64_t max_bytes);
DatalabThumbnailDecodeCompletion *datalab_thumbnail_decode_take_current(DatalabThumbnailDecode *decode);
void datalab_thumbnail_decode_completion_destroy(DatalabThumbnailDecodeCompletion *completion);
DatalabThumbnailDecodeStats datalab_thumbnail_decode_stats(DatalabThumbnailDecode *decode);

#endif
