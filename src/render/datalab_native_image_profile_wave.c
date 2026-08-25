#include "render/datalab_native_image_profile_wave.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int datalab_profile_wave_env_enabled(void) {
    const char *value = getenv("DATALAB_NATIVE_IMAGE_PROFILE_WAVE");
    return value && value[0] &&
           (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
            strcasecmp(value, "yes") == 0);
}

static uint64_t datalab_profile_wave_delta(uint64_t current, uint64_t baseline) {
    return current >= baseline ? current - baseline : current;
}

static DatalabNativeImageProfileDelta datalab_profile_wave_measure(
    const DatalabNativeImageCounters *current,
    const DatalabNativeImageCounters *baseline,
    const DatalabRenderPerfFrame *frame) {
    DatalabNativeImageProfileDelta delta = {0};
#define DATALAB_PROFILE_DELTA(field) \
    delta.counters.field = datalab_profile_wave_delta(current->field, baseline->field)
    DATALAB_PROFILE_DELTA(image_upload_count);
    DATALAB_PROFILE_DELTA(image_reuse_count);
    DATALAB_PROFILE_DELTA(overlay_upload_count);
    DATALAB_PROFILE_DELTA(overlay_reuse_count);
    DATALAB_PROFILE_DELTA(overlay_redraw_count);
    DATALAB_PROFILE_DELTA(overlay_redraw_reuse_count);
    DATALAB_PROFILE_DELTA(presentation_recreate_count);
    DATALAB_PROFILE_DELTA(presentation_seed_upload_count);
    DATALAB_PROFILE_DELTA(presentation_seed_upload_bytes);
#undef DATALAB_PROFILE_DELTA
    if (frame) {
        delta.software_submit_ms = frame->software_submit_ms;
        delta.compatibility_upload_ms = frame->compatibility_upload_ms;
        delta.present_sync_ms = frame->present_sync_ms;
        delta.total_present_ms = frame->total_present_ms;
    }
    return delta;
}

int datalab_native_image_profile_stage_passes(DatalabNativeImageProfileStage stage,
                                              const DatalabNativeImageProfileDelta *delta) {
    if (!delta) {
        return 0;
    }
    switch (stage) {
        case DATALAB_NATIVE_IMAGE_PROFILE_ZOOM_BURST:
        case DATALAB_NATIVE_IMAGE_PROFILE_PAN:
            return delta->counters.image_upload_count == 0u &&
                   delta->counters.image_reuse_count >= 1u &&
                   delta->counters.overlay_upload_count == 0u &&
                   delta->counters.overlay_redraw_count == 0u &&
                   delta->counters.presentation_recreate_count == 0u;
        case DATALAB_NATIVE_IMAGE_PROFILE_HUD_TOGGLE:
            return delta->counters.image_upload_count == 0u &&
                   delta->counters.image_reuse_count >= 1u &&
                   delta->counters.overlay_upload_count == 1u &&
                   delta->counters.overlay_redraw_count == 1u &&
                   delta->counters.presentation_recreate_count == 0u;
        case DATALAB_NATIVE_IMAGE_PROFILE_RESIZE:
            return delta->counters.image_upload_count == 0u &&
                   delta->counters.image_reuse_count >= 1u &&
                   delta->counters.presentation_recreate_count >= 1u &&
                   delta->counters.presentation_seed_upload_count >= 1u;
        case DATALAB_NATIVE_IMAGE_PROFILE_CONTENT_REPLACE:
            return delta->counters.image_upload_count == 1u &&
                   delta->counters.presentation_recreate_count == 0u;
        default:
            return 1;
    }
}

static const char *datalab_profile_wave_stage_name(DatalabNativeImageProfileStage stage) {
    switch (stage) {
        case DATALAB_NATIVE_IMAGE_PROFILE_ZOOM_BURST: return "zoom_burst";
        case DATALAB_NATIVE_IMAGE_PROFILE_PAN: return "pan";
        case DATALAB_NATIVE_IMAGE_PROFILE_HUD_TOGGLE: return "hud_toggle";
        case DATALAB_NATIVE_IMAGE_PROFILE_RESIZE: return "resize";
        case DATALAB_NATIVE_IMAGE_PROFILE_CONTENT_REPLACE: return "content_replace";
        case DATALAB_NATIVE_IMAGE_PROFILE_BASELINE: return "baseline";
        default: return "complete";
    }
}

static int datalab_profile_wave_push(const SDL_Event *event) {
    SDL_Event copy;
    if (!event) {
        return 0;
    }
    copy = *event;
    return SDL_PushEvent(&copy) >= 0;
}

static int datalab_profile_wave_queue_zoom(SDL_Window *window) {
    SDL_Event event = {0};
    int width = 0;
    int height = 0;
    int index;
    SDL_GetWindowSize(window, &width, &height);
    SDL_WarpMouseInWindow(window, width / 2, height / 2);
    event.type = SDL_MOUSEWHEEL;
    event.wheel.y = 1;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    for (index = 0; index < 5; ++index) {
        if (!datalab_profile_wave_push(&event)) {
            return 0;
        }
    }
    return 1;
}

static int datalab_profile_wave_queue_pan(SDL_Window *window,
                                          SDL_Renderer *renderer,
                                          DatalabAppState *app_state) {
    SDL_Event motion = {0};
    SDL_Event release = {0};
    int width = 0;
    int height = 0;
    int drawable_x = 0;
    int drawable_y = 0;
    SDL_GetWindowSize(window, &width, &height);
    if (!datalab_renderer_backend_map_window_to_drawable_point(window,
                                                               renderer,
                                                               width / 2,
                                                               height / 2,
                                                               &drawable_x,
                                                               &drawable_y) ||
        !datalab_raster_viewport_begin_drag(&app_state->raster_viewport,
                                            drawable_x,
                                            drawable_y)) {
        return 0;
    }
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = width / 2 + 40;
    motion.motion.y = height / 2 + 24;
    motion.motion.state = SDL_BUTTON_LMASK;
    release.type = SDL_MOUSEBUTTONUP;
    release.button.button = SDL_BUTTON_LEFT;
    release.button.x = motion.motion.x;
    release.button.y = motion.motion.y;
    return datalab_profile_wave_push(&motion) && datalab_profile_wave_push(&release);
}

static int datalab_profile_wave_queue_key(SDL_Keycode keycode) {
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = keycode;
    return datalab_profile_wave_push(&event);
}

static int datalab_profile_wave_queue_wake(void) {
    SDL_Event event = {0};
    event.type = SDL_USEREVENT;
    return datalab_profile_wave_push(&event);
}

void datalab_native_image_profile_wave_init(DatalabNativeImageProfileWave *wave,
                                            const DatalabFrame *frame) {
    if (!wave) {
        return;
    }
    memset(wave, 0, sizeof(*wave));
    wave->enabled = frame && frame->profile == DATALAB_PROFILE_IMAGE &&
                    datalab_profile_wave_env_enabled();
    wave->stage = DATALAB_NATIVE_IMAGE_PROFILE_BASELINE;
    if (frame) {
        wave->frame_override = *frame;
    }
}

const DatalabFrame *datalab_native_image_profile_wave_frame(
    const DatalabNativeImageProfileWave *wave,
    const DatalabFrame *fallback) {
    return wave && wave->frame_override_active ? &wave->frame_override : fallback;
}

int datalab_native_image_profile_wave_on_present(DatalabNativeImageProfileWave *wave,
                                                 SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 DatalabAppState *app_state) {
    DatalabNativeImageCounters current = {0};
    DatalabRenderPerfFrame perf = {0};
    DatalabNativeImageProfileDelta delta;
    int passed;
    if (!wave || !wave->enabled) {
        return 1;
    }
    if (wave->stage == DATALAB_NATIVE_IMAGE_PROFILE_COMPLETE) {
        return 1;
    }
    if (!datalab_renderer_backend_native_image_counters_snapshot(renderer, &current) ||
        !datalab_render_perf_diag_last_frame(&perf)) {
        return 0;
    }
    if (wave->stage == DATALAB_NATIVE_IMAGE_PROFILE_BASELINE) {
        wave->baseline = current;
        wave->stage = DATALAB_NATIVE_IMAGE_PROFILE_ZOOM_BURST;
        return datalab_profile_wave_queue_zoom(window);
    }
    delta = datalab_profile_wave_measure(&current, &wave->baseline, &perf);
    passed = datalab_native_image_profile_stage_passes(wave->stage, &delta);
    fprintf(stdout,
            "DATALAB_NATIVE_IMAGE_PROFILE schema=1 stage=%s status=%s image_upload_delta=%llu image_reuse_delta=%llu overlay_upload_delta=%llu overlay_reuse_delta=%llu overlay_redraw_delta=%llu overlay_redraw_reuse_delta=%llu presentation_recreate_delta=%llu presentation_seed_upload_delta=%llu presentation_seed_upload_bytes_delta=%llu software_ms=%.3f compatibility_upload_ms=%.3f present_sync_ms=%.3f total_present_ms=%.3f\n",
            datalab_profile_wave_stage_name(wave->stage),
            passed ? "pass" : "fail",
            (unsigned long long)delta.counters.image_upload_count,
            (unsigned long long)delta.counters.image_reuse_count,
            (unsigned long long)delta.counters.overlay_upload_count,
            (unsigned long long)delta.counters.overlay_reuse_count,
            (unsigned long long)delta.counters.overlay_redraw_count,
            (unsigned long long)delta.counters.overlay_redraw_reuse_count,
            (unsigned long long)delta.counters.presentation_recreate_count,
            (unsigned long long)delta.counters.presentation_seed_upload_count,
            (unsigned long long)delta.counters.presentation_seed_upload_bytes,
            delta.software_submit_ms,
            delta.compatibility_upload_ms,
            delta.present_sync_ms,
            delta.total_present_ms);
    fflush(stdout);
    if (!passed) {
        wave->failed = 1;
        return 0;
    }
    wave->baseline = current;
    switch (wave->stage) {
        case DATALAB_NATIVE_IMAGE_PROFILE_ZOOM_BURST:
            wave->stage = DATALAB_NATIVE_IMAGE_PROFILE_PAN;
            return datalab_profile_wave_queue_pan(window, renderer, app_state);
        case DATALAB_NATIVE_IMAGE_PROFILE_PAN:
            wave->stage = DATALAB_NATIVE_IMAGE_PROFILE_HUD_TOGGLE;
            return datalab_profile_wave_queue_key(SDLK_h);
        case DATALAB_NATIVE_IMAGE_PROFILE_HUD_TOGGLE: {
            int width = 0;
            int height = 0;
            wave->stage = DATALAB_NATIVE_IMAGE_PROFILE_RESIZE;
            SDL_GetWindowSize(window, &width, &height);
            SDL_SetWindowSize(window, width > 800 ? width - 96 : width + 96,
                              height > 600 ? height - 64 : height + 64);
            return datalab_profile_wave_queue_wake();
        }
        case DATALAB_NATIVE_IMAGE_PROFILE_RESIZE:
            wave->stage = DATALAB_NATIVE_IMAGE_PROFILE_CONTENT_REPLACE;
            wave->frame_override_active = 1;
            wave->frame_override.raster_content_generation =
                wave->frame_override.raster_content_generation == UINT64_MAX
                    ? 1u
                    : wave->frame_override.raster_content_generation + 1u;
            return datalab_profile_wave_queue_wake();
        case DATALAB_NATIVE_IMAGE_PROFILE_CONTENT_REPLACE: {
            SDL_Event quit = {0};
            wave->stage = DATALAB_NATIVE_IMAGE_PROFILE_COMPLETE;
            fprintf(stdout,
                    "DATALAB_NATIVE_IMAGE_PROFILE_WAVE schema=1 status=pass stages=5\n");
            quit.type = SDL_QUIT;
            return datalab_profile_wave_push(&quit);
        }
        default:
            return 1;
    }
}
