#include "app/datalab_async_decode.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <string.h>

#include "app/datalab_runtime_prefs.h"
#include "app/datalab_runtime_pack.h"
#include "datalab/datalab_app_main.h"
#include "data/input_file_loader.h"

typedef struct DatalabAsyncDecodeTask {
    DatalabAsyncDecode *decode;
    uint64_t generation;
    DatalabAsyncDecodePriority priority;
    char path[DATALAB_APP_PATH_CAP];
    DatalabImageIdentity identity;
} DatalabAsyncDecodeTask;

static void datalab_async_decode_completion_destroy(DatalabAsyncDecodeCompletion *completion) {
    if (!completion) {
        return;
    }
    datalab_frame_free(&completion->frame);
    core_free(completion);
}

static int datalab_async_decode_diag_enabled(void) {
    const char *value = getenv("DATALAB_ASYNC_DECODE_DIAG");
    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void datalab_async_decode_discard_pending_selected(DatalabAsyncDecode *decode) {
    if (!decode || !decode->pending_selected) {
        return;
    }
    datalab_async_decode_completion_destroy(decode->pending_selected);
    decode->pending_selected = NULL;
}

static void datalab_async_decode_set_error(DatalabAsyncDecode *decode, const char *message) {
    if (!decode) {
        return;
    }
    pthread_mutex_lock(&decode->mutex);
    (void)snprintf(decode->last_error,
                   sizeof(decode->last_error),
                   "%s",
                   message && message[0] != '\0' ? message : "async decode unavailable");
    pthread_mutex_unlock(&decode->mutex);
}

static int datalab_async_decode_task_is_current(DatalabAsyncDecode *decode, uint64_t generation) {
    int current = 0;
    if (!decode) {
        return 0;
    }
    pthread_mutex_lock(&decode->mutex);
    current = decode->initialized && !decode->shutting_down && decode->generation == generation;
    pthread_mutex_unlock(&decode->mutex);
    return current;
}

static void datalab_async_decode_forget_identity(DatalabAsyncDecode *decode,
                                                  uint64_t generation,
                                                  const DatalabImageIdentity *identity) {
    if (!decode || !identity) return;
    pthread_mutex_lock(&decode->mutex);
    for (size_t i = 0u; i < DATALAB_ASYNC_DECODE_TASK_CAPACITY; ++i) {
        if (decode->queued_identity_valid[i] && decode->queued_generations[i] == generation &&
            datalab_image_identity_equal(&decode->queued_identities[i], identity)) {
            decode->queued_identity_valid[i] = 0u;
            break;
        }
    }
    pthread_mutex_unlock(&decode->mutex);
}

static bool datalab_async_decode_external_signal(void *ctx) {
    DatalabAsyncDecode *decode = (DatalabAsyncDecode *)ctx;
    SDL_Event event;
    if (!decode || decode->wake_event_type == 0u) {
        return false;
    }
    memset(&event, 0, sizeof(event));
    event.type = decode->wake_event_type;
    event.user.type = decode->wake_event_type;
    return SDL_PushEvent(&event) >= 0;
}

static CoreWakeWaitResult datalab_async_decode_external_wait(void *ctx, uint32_t timeout_ms) {
    (void)ctx;
    (void)timeout_ms;
    /* DataLab's render loop owns SDL event draining. This callback exists only
     * to satisfy the external wake adapter contract; callers never consume
     * events through CoreWake directly. */
    return CORE_WAKE_WAIT_TIMEOUT;
}

static void *datalab_async_decode_task_run(void *task_ctx) {
    DatalabAsyncDecodeTask *task = (DatalabAsyncDecodeTask *)task_ctx;
    DatalabAsyncDecode *decode = task ? task->decode : NULL;
    DatalabAsyncDecodeCompletion *completion = NULL;
    if (!task || !decode) {
        core_free(task);
        return NULL;
    }
    if (datalab_async_decode_task_is_current(decode, task->generation)) {
        completion = (DatalabAsyncDecodeCompletion *)core_alloc(sizeof(*completion));
        if (completion) {
            memset(completion, 0, sizeof(*completion));
            datalab_frame_init(&completion->frame);
            completion->generation = task->generation;
            completion->priority = task->priority;
            (void)snprintf(completion->path, sizeof(completion->path), "%s", task->path);
            completion->identity = task->identity;
            completion->result = datalab_load_input_file(task->path, &completion->frame);
            if (completion->result.code == CORE_OK &&
                !datalab_image_residency_identity_is_current(&completion->identity)) {
                datalab_frame_free(&completion->frame);
                datalab_frame_init(&completion->frame);
                completion->result = (CoreResult){ CORE_ERR_IO, "input changed during async decode" };
            }
            if (!datalab_async_decode_task_is_current(decode, task->generation) ||
                !core_queue_mutex_push(&decode->completion_queue, completion)) {
                if (task->priority == DATALAB_ASYNC_DECODE_PRIORITY_SELECTED) {
                    pthread_mutex_lock(&decode->mutex);
                    decode->selected_stale_discard_count += 1u;
                    pthread_mutex_unlock(&decode->mutex);
                }
                datalab_async_decode_completion_destroy(completion);
            } else {
                (void)core_wake_signal(&decode->wake);
            }
        } else {
            datalab_async_decode_set_error(decode, "async decode allocation failed");
        }
    } else if (task->priority == DATALAB_ASYNC_DECODE_PRIORITY_SELECTED) {
        pthread_mutex_lock(&decode->mutex);
        decode->selected_stale_discard_count += 1u;
        pthread_mutex_unlock(&decode->mutex);
    }
    pthread_mutex_lock(&decode->mutex);
    if (decode->outstanding_count > 0u) {
        decode->outstanding_count -= 1u;
    }
    pthread_mutex_unlock(&decode->mutex);
    datalab_async_decode_forget_identity(decode, task->generation, &task->identity);
    core_free(task);
    return NULL;
}

static int datalab_async_decode_enqueue(DatalabAsyncDecode *decode,
                                        const char *path,
                                        uint64_t generation,
                                        DatalabAsyncDecodePriority priority) {
    DatalabAsyncDecodeTask *task = NULL;
    int accepted = 0;
    if (!decode || !path || path[0] == '\0' ||
        (!datalab_input_file_is_bmp(path) && !datalab_input_file_is_png(path))) {
        return 0;
    }
    task = (DatalabAsyncDecodeTask *)core_alloc(sizeof(*task));
    if (!task) {
        datalab_async_decode_set_error(decode, "async decode task allocation failed");
        return 0;
    }
    memset(task, 0, sizeof(*task));
    task->decode = decode;
    task->generation = generation;
    task->priority = priority;
    (void)snprintf(task->path, sizeof(task->path), "%s", path);
    if (!datalab_image_identity_from_path(path, &task->identity)) {
        core_free(task);
        datalab_async_decode_set_error(decode, "async decode identity capture failed");
        return 0;
    }

    pthread_mutex_lock(&decode->mutex);
    if (decode->initialized && !decode->shutting_down && decode->generation == generation &&
        decode->outstanding_count < DATALAB_ASYNC_DECODE_COMPLETION_CAPACITY) {
        size_t free_slot = DATALAB_ASYNC_DECODE_TASK_CAPACITY;
        int duplicate = 0;
        for (size_t i = 0u; i < DATALAB_ASYNC_DECODE_TASK_CAPACITY; ++i) {
            if (decode->queued_identity_valid[i] && decode->queued_generations[i] == generation &&
                datalab_image_identity_equal(&decode->queued_identities[i], &task->identity)) {
                decode->deduplicated_count += 1u;
                duplicate = 1;
                break;
            }
            if (!decode->queued_identity_valid[i] && free_slot == DATALAB_ASYNC_DECODE_TASK_CAPACITY) free_slot = i;
        }
        if (!duplicate && free_slot < DATALAB_ASYNC_DECODE_TASK_CAPACITY) {
            decode->queued_identities[free_slot] = task->identity;
            decode->queued_generations[free_slot] = generation;
            decode->queued_identity_valid[free_slot] = 1u;
            decode->outstanding_count += 1u;
            accepted = 1;
        }
    }
    pthread_mutex_unlock(&decode->mutex);
    if (!accepted || !core_workers_submit(&decode->workers, datalab_async_decode_task_run, task)) {
        if (accepted) {
            pthread_mutex_lock(&decode->mutex);
            if (decode->outstanding_count > 0u) {
                decode->outstanding_count -= 1u;
            }
            (void)snprintf(decode->last_error, sizeof(decode->last_error), "%s", "async decode queue is full");
            pthread_mutex_unlock(&decode->mutex);
            datalab_async_decode_forget_identity(decode, generation, &task->identity);
        }
        core_free(task);
        return 0;
    }
    return 1;
}

void datalab_async_decode_init(DatalabAsyncDecode *decode) {
    if (!decode) {
        return;
    }
    memset(decode, 0, sizeof(*decode));
    (void)pthread_mutex_init(&decode->mutex, NULL);
    decode->generation = 1u;
}

int datalab_async_decode_start(DatalabAsyncDecode *decode) {
    uint32_t event_type = 0u;
    if (!decode) {
        return 0;
    }
    pthread_mutex_lock(&decode->mutex);
    if (decode->initialized) {
        pthread_mutex_unlock(&decode->mutex);
        return 1;
    }
    pthread_mutex_unlock(&decode->mutex);
    event_type = SDL_RegisterEvents(1);
    if (event_type == (uint32_t)-1) {
        datalab_async_decode_set_error(decode, "failed to reserve SDL async decode wake event");
        return 0;
    }
    decode->wake_event_type = event_type;
    if (!core_queue_mutex_init_ex(&decode->completion_queue,
                                  decode->completion_backing,
                                  DATALAB_ASYNC_DECODE_COMPLETION_CAPACITY,
                                  CORE_QUEUE_OVERFLOW_REJECT) ||
        !core_wake_init_external(&decode->wake,
                                 datalab_async_decode_external_signal,
                                 datalab_async_decode_external_wait,
                                 decode) ||
        !core_workers_init(&decode->workers,
                           decode->thread_backing,
                           DATALAB_ASYNC_DECODE_WORKER_COUNT,
                           decode->task_backing,
                           DATALAB_ASYNC_DECODE_TASK_CAPACITY,
                           NULL)) {
        core_wake_shutdown(&decode->wake);
        core_queue_mutex_destroy(&decode->completion_queue);
        decode->wake_event_type = 0u;
        datalab_async_decode_set_error(decode, "failed to initialize async decode infrastructure");
        return 0;
    }
    pthread_mutex_lock(&decode->mutex);
    decode->shutting_down = 0;
    decode->initialized = 1;
    decode->last_error[0] = '\0';
    pthread_mutex_unlock(&decode->mutex);
    return 1;
}

void datalab_async_decode_cancel(DatalabAsyncDecode *decode) {
    if (!decode) {
        return;
    }
    pthread_mutex_lock(&decode->mutex);
    if (decode->generation != UINT64_MAX) {
        decode->generation += 1u;
    }
    pthread_mutex_unlock(&decode->mutex);
    datalab_async_decode_discard_pending_selected(decode);
}

void datalab_async_decode_shutdown(DatalabAsyncDecode *decode) {
    void *item = NULL;
    if (!decode) {
        return;
    }
    pthread_mutex_lock(&decode->mutex);
    if (!decode->initialized) {
        pthread_mutex_unlock(&decode->mutex);
        return;
    }
    decode->shutting_down = 1;
    if (decode->generation != UINT64_MAX) {
        decode->generation += 1u;
    }
    pthread_mutex_unlock(&decode->mutex);
    /* DRAIN is deliberate: CoreWorkers has no task-destructor callback, so it
     * preserves caller-owned task lifetime while stale generation checks make
     * the work observationally canceled. */
    core_workers_shutdown_with_mode(&decode->workers, CORE_WORKERS_SHUTDOWN_DRAIN);
    while (core_queue_mutex_pop(&decode->completion_queue, &item)) {
        datalab_async_decode_completion_destroy((DatalabAsyncDecodeCompletion *)item);
        item = NULL;
    }
    datalab_async_decode_discard_pending_selected(decode);
    core_wake_shutdown(&decode->wake);
    core_queue_mutex_destroy(&decode->completion_queue);
    pthread_mutex_lock(&decode->mutex);
    decode->initialized = 0;
    decode->outstanding_count = 0u;
    decode->wake_event_type = 0u;
    pthread_mutex_unlock(&decode->mutex);
}

int datalab_async_decode_request_selected(DatalabAsyncDecode *decode, const char *path) {
    uint64_t generation = 0u;
    if (!decode || !path || path[0] == '\0' ||
        (!datalab_input_file_is_bmp(path) && !datalab_input_file_is_png(path))) {
        return 0;
    }
    pthread_mutex_lock(&decode->mutex);
    if (decode->initialized && !decode->shutting_down && decode->generation != UINT64_MAX) {
        decode->generation += 1u;
        generation = decode->generation;
    }
    pthread_mutex_unlock(&decode->mutex);
    if (generation != 0u) {
        if (decode->pending_selected) {
            decode->selected_stale_discard_count += 1u;
        }
        datalab_async_decode_discard_pending_selected(decode);
        decode->selected_request_count += 1u;
    }
    return generation != 0u && datalab_async_decode_enqueue(decode,
                                                              path,
                                                              generation,
                                                              DATALAB_ASYNC_DECODE_PRIORITY_SELECTED);
}

int datalab_async_decode_request_neighbor(DatalabAsyncDecode *decode,
                                          const char *path,
                                          uint64_t generation) {
    return datalab_async_decode_enqueue(decode, path, generation, DATALAB_ASYNC_DECODE_PRIORITY_NEIGHBOR);
}

int datalab_async_decode_is_wake_event(const DatalabAsyncDecode *decode, uint32_t event_type) {
    return decode && decode->initialized && decode->wake_event_type != 0u && event_type == decode->wake_event_type;
}

uint64_t datalab_async_decode_current_generation(DatalabAsyncDecode *decode) {
    uint64_t generation = 0u;
    if (!decode) {
        return 0u;
    }
    pthread_mutex_lock(&decode->mutex);
    generation = decode->generation;
    pthread_mutex_unlock(&decode->mutex);
    return generation;
}

const char *datalab_async_decode_last_error(const DatalabAsyncDecode *decode) {
    return decode ? decode->last_error : "async decode unavailable";
}

const DatalabAsyncDecodeCompletion *datalab_async_decode_pending_selected(DatalabAsyncDecode *decode) {
    DatalabAsyncDecodeCompletion *pending = NULL;
    if (!decode) {
        return NULL;
    }
    pending = decode->pending_selected;
    if (!pending || !datalab_async_decode_task_is_current(decode, pending->generation)) {
        return NULL;
    }
    return pending;
}

int datalab_async_decode_commit_pending_selected(DatalabAsyncDecode *decode,
                                                 struct DatalabAppRuntime *runtime,
                                                 DatalabAppState *app_state) {
    DatalabAsyncDecodeCompletion *completion = NULL;
    if (!decode || !runtime || !app_state) {
        return 0;
    }
    completion = (DatalabAsyncDecodeCompletion *)datalab_async_decode_pending_selected(decode);
    if (!completion) {
        app_state->async_decode_frame_ready = 0;
        return 0;
    }
    decode->pending_selected = NULL;
    datalab_frame_free(&runtime->frame);
    runtime->frame = completion->frame;
    datalab_frame_init(&completion->frame);
    runtime->frame_loaded = 1;
    datalab_runtime_note_active_raster_content(runtime);
    (void)snprintf(runtime->selected_pack_path,
                   sizeof(runtime->selected_pack_path),
                   "%s",
                   completion->path);
    runtime->pack_path = runtime->selected_pack_path;
    runtime->last_load_error[0] = '\0';
    datalab_image_residency_note_active(runtime->image_residency, runtime->pack_path, &runtime->frame);
    /* Do not synchronously decode four neighbors after an interactive swap.
     * The selected frame is now present; extra I/O belongs to a separately
     * authorized residency/prefetch slice. */
    (void)datalab_runtime_prefs_save_last_opened_input_file(runtime->pack_path);
    app_state->pack_path = runtime->pack_path;
    app_state->profile = runtime->frame.profile;
    datalab_raster_viewport_request_reset(&app_state->raster_viewport);
    app_state->async_decode_frame_ready = 0;
    decode->selected_commit_count += 1u;
    if (datalab_async_decode_diag_enabled()) {
        fprintf(stderr,
                "datalab: async-decode commit generation=%llu requests=%llu completions=%llu stale=%llu\n",
                (unsigned long long)completion->generation,
                (unsigned long long)decode->selected_request_count,
                (unsigned long long)decode->selected_completion_count,
                (unsigned long long)decode->selected_stale_discard_count);
    }
    datalab_async_decode_completion_destroy(completion);
    return 1;
}

void datalab_async_decode_reject_pending_selected(DatalabAsyncDecode *decode,
                                                  struct DatalabAppRuntime *runtime,
                                                  DatalabAppState *app_state,
                                                  const char *detail) {
    if (!decode || !runtime || !app_state) {
        return;
    }
    if (decode->pending_selected) {
        datalab_async_decode_discard_pending_selected(decode);
        decode->selected_reject_count += 1u;
    }
    (void)snprintf(runtime->last_load_error,
                   sizeof(runtime->last_load_error),
                   "input display update failed: %s; keeping current frame (U/J skip, F5 retry, O picker)",
                   detail && detail[0] != '\0' ? detail : "renderer upload unavailable");
    app_state->async_decode_frame_ready = 0;
}

int datalab_async_decode_pump(DatalabAsyncDecode *decode,
                              struct DatalabAppRuntime *runtime,
                              DatalabAppState *app_state) {
    void *item = NULL;
    int applied = 0;
    if (!decode || !runtime || !app_state || !decode->initialized) {
        return 0;
    }
    while (core_queue_mutex_pop(&decode->completion_queue, &item)) {
        DatalabAsyncDecodeCompletion *completion = (DatalabAsyncDecodeCompletion *)item;
        uint64_t generation = datalab_async_decode_current_generation(decode);
        item = NULL;
        if (completion->priority == DATALAB_ASYNC_DECODE_PRIORITY_SELECTED &&
            completion->generation == generation && completion->result.code == CORE_OK &&
            completion->frame.profile == DATALAB_PROFILE_IMAGE && completion->frame.drawing_rgba &&
            runtime->image_residency &&
            datalab_image_residency_can_activate(runtime->image_residency, &completion->identity, &completion->frame)) {
            datalab_async_decode_discard_pending_selected(decode);
            decode->pending_selected = completion;
            completion = NULL;
            decode->selected_completion_count += 1u;
            app_state->async_decode_frame_ready = 1;
            applied = 1;
        } else if (completion->priority == DATALAB_ASYNC_DECODE_PRIORITY_SELECTED &&
                   completion->generation == generation && completion->result.code != CORE_OK) {
            (void)snprintf(runtime->last_load_error,
                           sizeof(runtime->last_load_error),
                           "input load failed: %s; keeping current frame (U/J skip, F5 retry, O picker)",
                           completion->result.message ? completion->result.message : "unknown error");
        } else if (completion->priority == DATALAB_ASYNC_DECODE_PRIORITY_SELECTED &&
                   completion->generation == generation) {
            if (runtime->image_residency) {
                if (!datalab_image_residency_identity_is_current(&completion->identity)) {
                    runtime->image_residency->cpu.stale_rejections++;
                } else {
                    runtime->image_residency->cpu.admission_rejections++;
                }
            }
            (void)snprintf(runtime->last_load_error,
                           sizeof(runtime->last_load_error),
                           "input load rejected: residency identity or CPU budget changed");
        } else if (completion->priority == DATALAB_ASYNC_DECODE_PRIORITY_SELECTED) {
            decode->selected_stale_discard_count += 1u;
        } else if (completion->priority == DATALAB_ASYNC_DECODE_PRIORITY_NEIGHBOR &&
                   completion->generation == generation && completion->result.code == CORE_OK &&
                   completion->frame.profile == DATALAB_PROFILE_IMAGE && runtime->image_residency) {
            (void)datalab_image_residency_store_cpu_identity(runtime->image_residency,
                                                              &completion->identity,
                                                              &completion->frame);
        }
        datalab_async_decode_completion_destroy(completion);
    }
    return applied;
}
