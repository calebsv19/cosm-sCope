#include "app/datalab_thumbnail_decode.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "core_queue.h"
#include "core_workers.h"
#include "data/input_file_loader.h"

enum {
    DATALAB_THUMBNAIL_DECODE_WORKERS = 1,
    DATALAB_THUMBNAIL_DECODE_TASK_CAPACITY = 1,
    DATALAB_THUMBNAIL_DECODE_COMPLETION_CAPACITY = 2
};

typedef struct DatalabThumbnailDecodeTask {
    DatalabThumbnailDecode *decode;
    uint64_t generation;
    DatalabImageIdentity identity;
    char path[DATALAB_APP_PATH_CAP];
    uint32_t max_edge;
    uint64_t max_bytes;
} DatalabThumbnailDecodeTask;

struct DatalabThumbnailDecode {
    pthread_t thread_backing[DATALAB_THUMBNAIL_DECODE_WORKERS];
    CoreWorkerTask task_backing[DATALAB_THUMBNAIL_DECODE_TASK_CAPACITY];
    void *completion_backing[DATALAB_THUMBNAIL_DECODE_COMPLETION_CAPACITY];
    CoreWorkers workers;
    CoreQueueMutex completion_queue;
    pthread_mutex_t mutex;
    DatalabImageIdentity desired_identity;
    uint64_t generation;
    uint64_t outstanding_generation;
    int initialized;
    int shutting_down;
    int outstanding;
    DatalabThumbnailDecodeStats stats;
};

void datalab_thumbnail_decode_completion_destroy(DatalabThumbnailDecodeCompletion *completion) {
    if (!completion) return;
    core_free(completion->rgba);
    core_free(completion);
}

static int datalab_thumbnail_decode_is_current(DatalabThumbnailDecode *decode, uint64_t generation) {
    int current = 0;
    pthread_mutex_lock(&decode->mutex);
    current = decode->initialized && !decode->shutting_down && decode->generation == generation;
    pthread_mutex_unlock(&decode->mutex);
    return current;
}

static int datalab_thumbnail_dimensions(uint32_t source_width,
                                        uint32_t source_height,
                                        uint32_t max_edge,
                                        uint64_t max_bytes,
                                        uint32_t *out_width,
                                        uint32_t *out_height) {
    uint32_t edge = 0u;
    if (!out_width || !out_height || source_width == 0u || source_height == 0u || max_edge == 0u) return 0;
    edge = source_width > source_height ? source_width : source_height;
    if (edge <= max_edge) {
        *out_width = source_width;
        *out_height = source_height;
    } else {
        *out_width = (uint32_t)(((uint64_t)source_width * max_edge + edge / 2u) / edge);
        *out_height = (uint32_t)(((uint64_t)source_height * max_edge + edge / 2u) / edge);
        if (*out_width == 0u) *out_width = 1u;
        if (*out_height == 0u) *out_height = 1u;
    }
    return datalab_image_rgba_bytes(*out_width, *out_height) <= max_bytes;
}

static uint8_t *datalab_thumbnail_downscale(const DatalabFrame *frame,
                                            uint32_t width,
                                            uint32_t height) {
    uint64_t bytes = datalab_image_rgba_bytes(width, height);
    uint8_t *rgba = NULL;
    if (!frame || !frame->drawing_rgba || bytes == 0u) return NULL;
    rgba = (uint8_t *)core_alloc((size_t)bytes);
    if (!rgba) return NULL;
    for (uint32_t y = 0u; y < height; ++y) {
        uint32_t source_y = (uint32_t)(((uint64_t)y * frame->height) / height);
        for (uint32_t x = 0u; x < width; ++x) {
            uint32_t source_x = (uint32_t)(((uint64_t)x * frame->width) / width);
            memcpy(rgba + (((size_t)y * width + x) * 4u),
                   frame->drawing_rgba + (((size_t)source_y * frame->width + source_x) * 4u),
                   4u);
        }
    }
    return rgba;
}

static void *datalab_thumbnail_decode_task_run(void *task_context) {
    DatalabThumbnailDecodeTask *task = (DatalabThumbnailDecodeTask *)task_context;
    DatalabThumbnailDecode *decode = task ? task->decode : NULL;
    DatalabThumbnailDecodeCompletion *completion = NULL;
    DatalabFrame frame;
    if (!task || !decode) {
        core_free(task);
        return NULL;
    }
    datalab_frame_init(&frame);
    if (datalab_thumbnail_decode_is_current(decode, task->generation)) {
        completion = (DatalabThumbnailDecodeCompletion *)core_alloc(sizeof(*completion));
        if (completion) {
            memset(completion, 0, sizeof(*completion));
            completion->generation = task->generation;
            completion->identity = task->identity;
            completion->result = datalab_load_input_file(task->path, &frame);
            if (completion->result.code == CORE_OK &&
                (frame.profile != DATALAB_PROFILE_IMAGE || !frame.drawing_rgba ||
                 !datalab_thumbnail_dimensions(frame.width, frame.height,
                                               task->max_edge, task->max_bytes,
                                               &completion->width, &completion->height))) {
                completion->result = (CoreResult){ CORE_ERR_INVALID_ARG, "image cannot produce a bounded thumbnail" };
            }
            if (completion->result.code == CORE_OK) {
                completion->rgba = datalab_thumbnail_downscale(&frame, completion->width, completion->height);
                if (!completion->rgba) {
                    completion->result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "thumbnail allocation failed" };
                }
            }
            datalab_frame_free(&frame);
            if (!datalab_thumbnail_decode_is_current(decode, task->generation) ||
                !datalab_image_residency_identity_is_current(&completion->identity)) {
                pthread_mutex_lock(&decode->mutex);
                decode->stats.stale_discard_count += 1u;
                pthread_mutex_unlock(&decode->mutex);
                datalab_thumbnail_decode_completion_destroy(completion);
                completion = NULL;
            } else if (!core_queue_mutex_push(&decode->completion_queue, completion)) {
                pthread_mutex_lock(&decode->mutex);
                decode->stats.rejected_count += 1u;
                pthread_mutex_unlock(&decode->mutex);
                datalab_thumbnail_decode_completion_destroy(completion);
                completion = NULL;
            } else {
                pthread_mutex_lock(&decode->mutex);
                decode->stats.completed_count += 1u;
                pthread_mutex_unlock(&decode->mutex);
            }
        } else {
            datalab_frame_free(&frame);
            pthread_mutex_lock(&decode->mutex);
            decode->stats.rejected_count += 1u;
            pthread_mutex_unlock(&decode->mutex);
        }
    }
    pthread_mutex_lock(&decode->mutex);
    if (decode->outstanding && decode->outstanding_generation == task->generation) {
        decode->outstanding = 0;
    }
    pthread_mutex_unlock(&decode->mutex);
    core_free(task);
    return NULL;
}

DatalabThumbnailDecode *datalab_thumbnail_decode_create(void) {
    DatalabThumbnailDecode *decode = (DatalabThumbnailDecode *)core_alloc(sizeof(*decode));
    if (!decode) return NULL;
    memset(decode, 0, sizeof(*decode));
    if (pthread_mutex_init(&decode->mutex, NULL) != 0) {
        core_free(decode);
        return NULL;
    }
    decode->generation = 1u;
    if (!core_queue_mutex_init_ex(&decode->completion_queue,
                                  decode->completion_backing,
                                  DATALAB_THUMBNAIL_DECODE_COMPLETION_CAPACITY,
                                  CORE_QUEUE_OVERFLOW_REJECT)) {
        pthread_mutex_destroy(&decode->mutex);
        core_free(decode);
        return NULL;
    }
    if (!core_workers_init(&decode->workers,
                           decode->thread_backing,
                           DATALAB_THUMBNAIL_DECODE_WORKERS,
                           decode->task_backing,
                           DATALAB_THUMBNAIL_DECODE_TASK_CAPACITY,
                           NULL)) {
        core_queue_mutex_destroy(&decode->completion_queue);
        pthread_mutex_destroy(&decode->mutex);
        core_free(decode);
        return NULL;
    }
    decode->initialized = 1;
    return decode;
}

void datalab_thumbnail_decode_cancel(DatalabThumbnailDecode *decode) {
    if (!decode) return;
    pthread_mutex_lock(&decode->mutex);
    if (decode->generation != UINT64_MAX) decode->generation += 1u;
    memset(&decode->desired_identity, 0, sizeof(decode->desired_identity));
    pthread_mutex_unlock(&decode->mutex);
}

void datalab_thumbnail_decode_destroy(DatalabThumbnailDecode *decode) {
    void *item = NULL;
    if (!decode) return;
    pthread_mutex_lock(&decode->mutex);
    decode->shutting_down = 1;
    if (decode->generation != UINT64_MAX) decode->generation += 1u;
    pthread_mutex_unlock(&decode->mutex);
    core_workers_shutdown_with_mode(&decode->workers, CORE_WORKERS_SHUTDOWN_DRAIN);
    while (core_queue_mutex_pop(&decode->completion_queue, &item)) {
        datalab_thumbnail_decode_completion_destroy((DatalabThumbnailDecodeCompletion *)item);
        item = NULL;
    }
    core_queue_mutex_destroy(&decode->completion_queue);
    pthread_mutex_destroy(&decode->mutex);
    core_free(decode);
}

int datalab_thumbnail_decode_request(DatalabThumbnailDecode *decode,
                                     const char *path,
                                     uint32_t max_edge,
                                     uint64_t max_bytes) {
    DatalabThumbnailDecodeTask *task = NULL;
    DatalabImageIdentity identity;
    uint64_t generation = 0u;
    if (!decode || !path || max_edge == 0u || max_bytes == 0u ||
        (!datalab_input_file_is_bmp(path) && !datalab_input_file_is_png(path)) ||
        !datalab_image_identity_from_path(path, &identity)) return 0;

    pthread_mutex_lock(&decode->mutex);
    if (!decode->initialized || decode->shutting_down) {
        pthread_mutex_unlock(&decode->mutex);
        return 0;
    }
    if (!datalab_image_identity_equal(&decode->desired_identity, &identity)) {
        if (decode->generation != UINT64_MAX) decode->generation += 1u;
        decode->desired_identity = identity;
        decode->stats.selection_count += 1u;
    } else if (decode->outstanding && decode->outstanding_generation == decode->generation) {
        decode->stats.deduplicated_count += 1u;
        pthread_mutex_unlock(&decode->mutex);
        return 1;
    }
    generation = decode->generation;
    if (decode->outstanding) {
        pthread_mutex_unlock(&decode->mutex);
        return 1;
    }
    decode->outstanding = 1;
    decode->outstanding_generation = generation;
    if (decode->stats.peak_outstanding < 1u) decode->stats.peak_outstanding = 1u;
    pthread_mutex_unlock(&decode->mutex);

    task = (DatalabThumbnailDecodeTask *)core_alloc(sizeof(*task));
    if (task) {
        memset(task, 0, sizeof(*task));
        task->decode = decode;
        task->generation = generation;
        task->identity = identity;
        task->max_edge = max_edge;
        task->max_bytes = max_bytes;
        (void)snprintf(task->path, sizeof(task->path), "%s", identity.canonical_path);
    }
    if (!task || !core_workers_submit(&decode->workers, datalab_thumbnail_decode_task_run, task)) {
        core_free(task);
        pthread_mutex_lock(&decode->mutex);
        if (decode->outstanding && decode->outstanding_generation == generation) decode->outstanding = 0;
        decode->stats.rejected_count += 1u;
        pthread_mutex_unlock(&decode->mutex);
        return 0;
    }
    pthread_mutex_lock(&decode->mutex);
    decode->stats.submitted_count += 1u;
    pthread_mutex_unlock(&decode->mutex);
    return 1;
}

DatalabThumbnailDecodeCompletion *datalab_thumbnail_decode_take_current(DatalabThumbnailDecode *decode) {
    void *item = NULL;
    DatalabThumbnailDecodeCompletion *current = NULL;
    if (!decode) return NULL;
    while (core_queue_mutex_pop(&decode->completion_queue, &item)) {
        DatalabThumbnailDecodeCompletion *candidate = (DatalabThumbnailDecodeCompletion *)item;
        item = NULL;
        if (datalab_thumbnail_decode_is_current(decode, candidate->generation) &&
            datalab_image_identity_equal(&decode->desired_identity, &candidate->identity)) {
            datalab_thumbnail_decode_completion_destroy(current);
            current = candidate;
        } else {
            pthread_mutex_lock(&decode->mutex);
            decode->stats.stale_discard_count += 1u;
            pthread_mutex_unlock(&decode->mutex);
            datalab_thumbnail_decode_completion_destroy(candidate);
        }
    }
    return current;
}

DatalabThumbnailDecodeStats datalab_thumbnail_decode_stats(DatalabThumbnailDecode *decode) {
    DatalabThumbnailDecodeStats stats = {0};
    if (!decode) return stats;
    pthread_mutex_lock(&decode->mutex);
    stats = decode->stats;
    pthread_mutex_unlock(&decode->mutex);
    return stats;
}
