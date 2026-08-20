#ifndef DATALAB_ASYNC_DECODE_H
#define DATALAB_ASYNC_DECODE_H

#include <stddef.h>
#include <stdint.h>

#include <pthread.h>

#include "core_queue.h"
#include "core_wake.h"
#include "core_workers.h"

#include "app/app_state.h"
#include "app/datalab_image_residency.h"
#include "data/pack_loader.h"

struct DatalabAppRuntime;

enum {
    DATALAB_ASYNC_DECODE_WORKER_COUNT = 1,
    DATALAB_ASYNC_DECODE_TASK_CAPACITY = 5,
    DATALAB_ASYNC_DECODE_COMPLETION_CAPACITY = 5
};

typedef enum DatalabAsyncDecodePriority {
    DATALAB_ASYNC_DECODE_PRIORITY_SELECTED = 0,
    DATALAB_ASYNC_DECODE_PRIORITY_NEIGHBOR = 1
} DatalabAsyncDecodePriority;

typedef struct DatalabAsyncDecodeCompletion {
    uint64_t generation;
    DatalabAsyncDecodePriority priority;
    char path[DATALAB_APP_PATH_CAP];
    DatalabImageIdentity identity;
    DatalabFrame frame;
    CoreResult result;
} DatalabAsyncDecodeCompletion;

typedef struct DatalabAsyncDecode {
    pthread_t thread_backing[DATALAB_ASYNC_DECODE_WORKER_COUNT];
    CoreWorkerTask task_backing[DATALAB_ASYNC_DECODE_TASK_CAPACITY];
    void *completion_backing[DATALAB_ASYNC_DECODE_COMPLETION_CAPACITY];
    CoreWorkers workers;
    CoreQueueMutex completion_queue;
    CoreWake wake;
    pthread_mutex_t mutex;
    /* Main/render-thread-owned candidate. It is never adopted into runtime
     * state until the renderer has presented its staged texture. */
    DatalabAsyncDecodeCompletion *pending_selected;
    uint64_t generation;
    size_t outstanding_count;
    uint64_t selected_request_count;
    uint64_t selected_completion_count;
    uint64_t selected_stale_discard_count;
    uint64_t selected_commit_count;
    uint64_t selected_reject_count;
    uint64_t deduplicated_count;
    DatalabImageIdentity queued_identities[DATALAB_ASYNC_DECODE_TASK_CAPACITY];
    uint64_t queued_generations[DATALAB_ASYNC_DECODE_TASK_CAPACITY];
    uint8_t queued_identity_valid[DATALAB_ASYNC_DECODE_TASK_CAPACITY];
    uint32_t wake_event_type;
    int initialized;
    int shutting_down;
    char last_error[160];
} DatalabAsyncDecode;

void datalab_async_decode_init(DatalabAsyncDecode *decode);
int datalab_async_decode_start(DatalabAsyncDecode *decode);
void datalab_async_decode_cancel(DatalabAsyncDecode *decode);
void datalab_async_decode_shutdown(DatalabAsyncDecode *decode);

int datalab_async_decode_request_selected(DatalabAsyncDecode *decode, const char *path);
int datalab_async_decode_request_neighbor(DatalabAsyncDecode *decode,
                                          const char *path,
                                          uint64_t generation);
int datalab_async_decode_is_wake_event(const DatalabAsyncDecode *decode, uint32_t event_type);
int datalab_async_decode_pump(DatalabAsyncDecode *decode,
                              struct DatalabAppRuntime *runtime,
                              DatalabAppState *app_state);
const DatalabAsyncDecodeCompletion *datalab_async_decode_pending_selected(DatalabAsyncDecode *decode);
int datalab_async_decode_commit_pending_selected(DatalabAsyncDecode *decode,
                                                 struct DatalabAppRuntime *runtime,
                                                 DatalabAppState *app_state);
void datalab_async_decode_reject_pending_selected(DatalabAsyncDecode *decode,
                                                  struct DatalabAppRuntime *runtime,
                                                  DatalabAppState *app_state,
                                                  const char *detail);
uint64_t datalab_async_decode_current_generation(DatalabAsyncDecode *decode);
const char *datalab_async_decode_last_error(const DatalabAsyncDecode *decode);

#endif
