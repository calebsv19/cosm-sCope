#ifndef DATALAB_NATIVE_IMAGE_PROFILE_WAVE_H
#define DATALAB_NATIVE_IMAGE_PROFILE_WAVE_H

#include <SDL2/SDL.h>

#include <stdint.h>

#include "app/app_state.h"
#include "data/pack_loader.h"
#include "render/datalab_render_perf_diag.h"
#include "render/datalab_renderer_backend.h"

typedef enum DatalabNativeImageProfileStage {
    DATALAB_NATIVE_IMAGE_PROFILE_BASELINE = 0,
    DATALAB_NATIVE_IMAGE_PROFILE_ZOOM_BURST,
    DATALAB_NATIVE_IMAGE_PROFILE_PAN,
    DATALAB_NATIVE_IMAGE_PROFILE_HUD_TOGGLE,
    DATALAB_NATIVE_IMAGE_PROFILE_RESIZE,
    DATALAB_NATIVE_IMAGE_PROFILE_CONTENT_REPLACE,
    DATALAB_NATIVE_IMAGE_PROFILE_COMPLETE
} DatalabNativeImageProfileStage;

typedef struct DatalabNativeImageProfileDelta {
    DatalabNativeImageCounters counters;
    double software_submit_ms;
    double compatibility_upload_ms;
    double present_sync_ms;
    double total_present_ms;
} DatalabNativeImageProfileDelta;

typedef struct DatalabNativeImageProfileWave {
    int enabled;
    int failed;
    DatalabNativeImageProfileStage stage;
    DatalabNativeImageCounters baseline;
    DatalabFrame frame_override;
    int frame_override_active;
} DatalabNativeImageProfileWave;

int datalab_native_image_profile_stage_passes(DatalabNativeImageProfileStage stage,
                                              const DatalabNativeImageProfileDelta *delta);
void datalab_native_image_profile_wave_init(DatalabNativeImageProfileWave *wave,
                                            const DatalabFrame *frame);
const DatalabFrame *datalab_native_image_profile_wave_frame(
    const DatalabNativeImageProfileWave *wave,
    const DatalabFrame *fallback);
int datalab_native_image_profile_wave_on_present(DatalabNativeImageProfileWave *wave,
                                                 SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 DatalabAppState *app_state);

#endif
