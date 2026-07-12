#include "render/render_view.h"
#include "render/render_view_internal.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

struct DatalabRenderSession {
    SDL_Window *window;
    SDL_Renderer *renderer;
    DatalabRasterTextureState raster_texture_state;
    int sdl_owner;
};

static DatalabRenderFailureDiagnostic g_datalab_render_failure_diag = {0};
static char g_datalab_render_failure_summary[256];
static uint64_t g_datalab_render_failure_sequence = 0u;

static const char *datalab_render_profile_name(DatalabProfile profile) {
    switch (profile) {
        case DATALAB_PROFILE_PHYSICS:
            return "physics";
        case DATALAB_PROFILE_DAW:
            return "daw";
        case DATALAB_PROFILE_TRACE:
            return "trace";
        case DATALAB_PROFILE_SKETCH:
            return "sketch";
        case DATALAB_PROFILE_IMAGE:
            return "image";
        case DATALAB_PROFILE_VOLUME:
            return "volume";
        case DATALAB_PROFILE_GROWTH:
            return "growth";
        case DATALAB_PROFILE_LINE_DIAGNOSTIC:
            return "line_diagnostic";
        default:
            return "unknown";
    }
}

static void datalab_render_set_failure_diagnostic(const char *stage,
                                                  const char *route,
                                                  DatalabProfile profile,
                                                  CoreResult result) {
    g_datalab_render_failure_sequence += 1u;
    (void)snprintf(g_datalab_render_failure_diag.stage,
                   sizeof(g_datalab_render_failure_diag.stage),
                   "%s",
                   (stage && stage[0] != '\0') ? stage : "unknown");
    (void)snprintf(g_datalab_render_failure_diag.route,
                   sizeof(g_datalab_render_failure_diag.route),
                   "%s",
                   (route && route[0] != '\0') ? route : "unknown");
    (void)snprintf(g_datalab_render_failure_diag.profile,
                   sizeof(g_datalab_render_failure_diag.profile),
                   "%s",
                   datalab_render_profile_name(profile));
    (void)snprintf(g_datalab_render_failure_diag.detail,
                   sizeof(g_datalab_render_failure_diag.detail),
                   "%s",
                   result.message ? result.message : "unknown");
    g_datalab_render_failure_diag.result_code = (int)result.code;
    g_datalab_render_failure_diag.sequence = g_datalab_render_failure_sequence;
    (void)snprintf(g_datalab_render_failure_summary,
                   sizeof(g_datalab_render_failure_summary),
                   "stage=%s route=%s profile=%s code=%d detail=%s",
                   g_datalab_render_failure_diag.stage,
                   g_datalab_render_failure_diag.route,
                   g_datalab_render_failure_diag.profile,
                   g_datalab_render_failure_diag.result_code,
                   g_datalab_render_failure_diag.detail);
}

const DatalabRenderFailureDiagnostic *datalab_render_last_failure_diagnostic(void) {
    return &g_datalab_render_failure_diag;
}

void datalab_render_clear_failure_diagnostic(void) {
    memset(&g_datalab_render_failure_diag, 0, sizeof(g_datalab_render_failure_diag));
    g_datalab_render_failure_summary[0] = '\0';
}

const char *datalab_render_last_failure_summary(void) {
    return g_datalab_render_failure_summary;
}

static CoreResult datalab_render_validate_frame(const DatalabFrame *frame, const DatalabAppState *app_state) {
    if (!frame || !app_state) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    if (frame->profile == DATALAB_PROFILE_PHYSICS || frame->profile == DATALAB_PROFILE_VOLUME || frame->profile == DATALAB_PROFILE_GROWTH) {
        if (!frame->density || !frame->velx || !frame->vely || frame->width == 0 || frame->height == 0) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid field frame" };
        }
        if (frame->profile == DATALAB_PROFILE_VOLUME &&
            (frame->volume_depth == 0u || frame->volume_slice_index >= frame->volume_depth)) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid volume slice frame" };
        }
        return core_result_ok();
    }
    if (frame->profile == DATALAB_PROFILE_DAW) {
        if (!frame->wave_min || !frame->wave_max || frame->point_count == 0) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid daw frame" };
        }
        return core_result_ok();
    }
    if (frame->profile == DATALAB_PROFILE_TRACE) {
        if (!frame->trace_samples || frame->trace_sample_count == 0) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid trace frame" };
        }
        return core_result_ok();
    }
    if (frame->profile == DATALAB_PROFILE_SKETCH || frame->profile == DATALAB_PROFILE_IMAGE) {
        if (!frame->drawing_rgba || frame->width == 0 || frame->height == 0) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid raster frame" };
        }
        return core_result_ok();
    }
    if (frame->profile == DATALAB_PROFILE_LINE_DIAGNOSTIC) {
        if (!frame->line_anchors || !frame->line_walls || frame->line_anchor_count == 0u) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "line diagnostic profile has no preview geometry" };
        }
        return core_result_ok();
    }
    return (CoreResult){ CORE_ERR_INVALID_ARG, "unknown frame profile" };
}

static CoreResult datalab_render_capture_surface(SDL_Renderer *renderer, const char *output_path) {
    SDL_Surface *surface = NULL;
    void *pixels = NULL;
    int width = 0;
    int height = 0;
    int pitch = 0;
    size_t blank_count = 0u;
    size_t pixel_count = 0u;
    if (!renderer || !output_path || output_path[0] == '\0') {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid visual artifact request" };
    }
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch) != 0) {
        CoreResult result = { CORE_ERR_IO, SDL_GetError() };
        SDL_FreeSurface(surface);
        return result;
    }
    pixels = surface->pixels;
    pitch = surface->pitch;
    pixel_count = (size_t)width * (size_t)height;
    for (int y = 0; y < height; ++y) {
        const uint32_t *row = (const uint32_t *)((const uint8_t *)pixels + ((size_t)y * (size_t)pitch));
        for (int x = 0; x < width; ++x) {
            if ((row[x] & 0x00ffffffu) == 0u) {
                blank_count += 1u;
            }
        }
    }
    if (blank_count == pixel_count) {
        SDL_FreeSurface(surface);
        return (CoreResult){ CORE_ERR_INVALID_ARG, "captured visual artifact is blank" };
    }
    if (SDL_SaveBMP(surface, output_path) != 0) {
        CoreResult result = { CORE_ERR_IO, SDL_GetError() };
        SDL_FreeSurface(surface);
        return result;
    }
    SDL_FreeSurface(surface);
    return core_result_ok();
}

static CoreResult datalab_render_submit_first_frame(DatalabRenderSession *session,
                                                    const DatalabFrame *frame,
                                                    DatalabAppState *app_state) {
    DatalabRenderSubmitOutcome outcome = {0};
    if (!session || !session->window || !session->renderer || !frame || !app_state) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid first-frame render request" };
    }
    if (frame->profile == DATALAB_PROFILE_DAW) {
        DatalabRenderDeriveFrame derive;
        datalab_render_derive_frame(frame, app_state, &derive);
        datalab_daw_render_submit_frame(session->window, session->renderer, frame, app_state, &derive, &outcome);
        return outcome.result;
    }
    if (frame->profile == DATALAB_PROFILE_TRACE) {
        DatalabRenderDeriveFrame derive;
        datalab_render_derive_frame(frame, app_state, &derive);
        datalab_trace_render_submit_frame(session->window, session->renderer, frame, app_state, &derive, &outcome);
        return outcome.result;
    }
    if (frame->profile == DATALAB_PROFILE_SKETCH || frame->profile == DATALAB_PROFILE_IMAGE) {
        DatalabSketchRenderDeriveFrame derive;
        CoreResult state_r = datalab_raster_texture_state_prepare(session->renderer,
                                                                  frame->width,
                                                                  frame->height,
                                                                  &session->raster_texture_state);
        if (state_r.code != CORE_OK) {
            return state_r;
        }
        datalab_raster_texture_state_begin_frame(&session->raster_texture_state);
        datalab_sketch_render_derive_frame(session->renderer, frame, app_state, &derive);
        datalab_sketch_render_submit_frame(session->window,
                                           session->renderer,
                                           &session->raster_texture_state,
                                           frame,
                                           app_state,
                                           &derive,
                                           &outcome);
        return outcome.result;
    }
    if (frame->profile == DATALAB_PROFILE_LINE_DIAGNOSTIC) {
        return datalab_line_diagnostic_submit_frame(session->window, session->renderer, frame, &outcome);
    }
    {
        const size_t sample_count = (size_t)frame->width * (size_t)frame->height;
        const size_t rgba_size = sample_count * 4u;
        SDL_Texture *texture = SDL_CreateTexture(session->renderer,
                                                 SDL_PIXELFORMAT_RGBA32,
                                                 SDL_TEXTUREACCESS_STREAMING,
                                                 (int)frame->width,
                                                 (int)frame->height);
        uint8_t *density_rgba = NULL;
        uint8_t *speed_rgba = NULL;
        float *speed = NULL;
        KitVizVecSegment *segments = NULL;
        DatalabPhysicsRenderDeriveFrame derive;
        CoreResult result = core_result_ok();
        if (!texture) {
            return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
        }
        density_rgba = (uint8_t *)core_alloc(rgba_size);
        speed_rgba = (uint8_t *)core_alloc(rgba_size);
        speed = (float *)core_alloc(sample_count * sizeof(float));
        segments = (KitVizVecSegment *)core_alloc(sample_count * sizeof(KitVizVecSegment));
        if (!density_rgba || !speed_rgba || !speed || !segments) {
            result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto physics_done;
        }
        {
            KitVizFieldStats dens_stats;
            KitVizFieldStats speed_stats;
            result = kit_viz_compute_field_stats(frame->density, frame->width, frame->height, &dens_stats);
            if (result.code != CORE_OK) {
                goto physics_done;
            }
            for (size_t i = 0; i < sample_count; ++i) {
                const float x = frame->velx[i];
                const float y = frame->vely[i];
                speed[i] = sqrtf((x * x) + (y * y));
            }
            result = kit_viz_compute_field_stats(speed, frame->width, frame->height, &speed_stats);
            if (result.code != CORE_OK) {
                goto physics_done;
            }
            result = kit_viz_build_heatmap_rgba(frame->density,
                                                frame->width,
                                                frame->height,
                                                dens_stats.min_value,
                                                dens_stats.max_value,
                                                KIT_VIZ_COLORMAP_HEAT,
                                                density_rgba,
                                                rgba_size);
            if (result.code != CORE_OK) {
                goto physics_done;
            }
            result = kit_viz_build_heatmap_rgba(speed,
                                                frame->width,
                                                frame->height,
                                                speed_stats.min_value,
                                                speed_stats.max_value,
                                                KIT_VIZ_COLORMAP_HEAT,
                                                speed_rgba,
                                                rgba_size);
            if (result.code != CORE_OK) {
                goto physics_done;
            }
        }
        datalab_physics_render_derive_frame(session->renderer,
                                            frame,
                                            app_state,
                                            density_rgba,
                                            speed_rgba,
                                            &derive);
        datalab_physics_render_submit_frame(session->window,
                                            session->renderer,
                                            texture,
                                            frame,
                                            app_state,
                                            segments,
                                            sample_count,
                                            &derive,
                                            &outcome);
        result = outcome.result;

physics_done:
        core_free(density_rgba);
        core_free(speed_rgba);
        core_free(speed);
        core_free(segments);
        SDL_DestroyTexture(texture);
        return result;
    }
}

CoreResult datalab_render_session_open(DatalabRenderSession **out_session) {
    DatalabRenderSession *session = NULL;
    const uint32_t video_mask = SDL_INIT_VIDEO;
    const uint32_t window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    CoreResult result = core_result_ok();
    if (!out_session) {
        result = (CoreResult){ CORE_ERR_INVALID_ARG, "invalid render session request" };
        datalab_render_set_failure_diagnostic("session_open", "validate", DATALAB_PROFILE_UNKNOWN, result);
        return result;
    }
    *out_session = NULL;
    session = (DatalabRenderSession *)SDL_calloc(1, sizeof(*session));
    if (!session) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "failed to allocate render session" };
        datalab_render_set_failure_diagnostic("session_open", "allocate", DATALAB_PROFILE_UNKNOWN, result);
        return result;
    }
    if ((SDL_WasInit(video_mask) & video_mask) == 0u) {
        if (SDL_Init(video_mask) != 0) {
            result = (CoreResult){ CORE_ERR_IO, SDL_GetError() };
            SDL_free(session);
            datalab_render_set_failure_diagnostic("session_open", "sdl_init", DATALAB_PROFILE_UNKNOWN, result);
            return result;
        }
        session->sdl_owner = 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    session->window = SDL_CreateWindow("DataLab",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       1200,
                                       900,
                                       (int)window_flags);
    if (!session->window) {
        result = (CoreResult){ CORE_ERR_IO, SDL_GetError() };
        datalab_render_session_close(session);
        datalab_render_set_failure_diagnostic("session_open", "create_window", DATALAB_PROFILE_UNKNOWN, result);
        return result;
    }
    session->renderer = SDL_CreateRenderer(session->window,
                                           -1,
                                           SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!session->renderer) {
        result = (CoreResult){ CORE_ERR_IO, SDL_GetError() };
        datalab_render_session_close(session);
        datalab_render_set_failure_diagnostic("session_open", "create_renderer", DATALAB_PROFILE_UNKNOWN, result);
        return result;
    }
    (void)datalab_text_renderer_init();
    *out_session = session;
    return core_result_ok();
}

void datalab_render_session_close(DatalabRenderSession *session) {
    if (!session) {
        return;
    }
    datalab_raster_texture_state_destroy(&session->raster_texture_state);
    if (session->renderer) {
        SDL_DestroyRenderer(session->renderer);
    }
    if (session->window) {
        SDL_DestroyWindow(session->window);
    }
    datalab_text_renderer_shutdown();
    if (session->sdl_owner) {
        SDL_Quit();
    }
    SDL_free(session);
}

CoreResult datalab_render_run_with_session(DatalabRenderSession *session,
                                           const DatalabFrame *frame,
                                           DatalabAppState *app_state) {
    CoreResult validate_r = datalab_render_validate_frame(frame, app_state);
    CoreResult run_r = core_result_ok();
    DatalabProfile profile = frame ? frame->profile : DATALAB_PROFILE_UNKNOWN;
    if (validate_r.code != CORE_OK) {
        datalab_render_set_failure_diagnostic("render_submit", "validate_frame", profile, validate_r);
        return validate_r;
    }
    if (!session || !session->window || !session->renderer) {
        run_r = (CoreResult){ CORE_ERR_INVALID_ARG, "render session is not open" };
        datalab_render_set_failure_diagnostic("render_submit", "session_state", profile, run_r);
        return run_r;
    }
    if (frame->profile == DATALAB_PROFILE_DAW) {
        run_r = render_daw_loop(session->window, session->renderer, frame, app_state);
        if (run_r.code != CORE_OK) {
            datalab_render_set_failure_diagnostic("render_submit", "profile_loop:daw", profile, run_r);
        }
        return run_r;
    }
    if (frame->profile == DATALAB_PROFILE_TRACE) {
        run_r = render_trace_loop(session->window, session->renderer, frame, app_state);
        if (run_r.code != CORE_OK) {
            datalab_render_set_failure_diagnostic("render_submit", "profile_loop:trace", profile, run_r);
        }
        return run_r;
    }
    if (frame->profile == DATALAB_PROFILE_SKETCH || frame->profile == DATALAB_PROFILE_IMAGE) {
        CoreResult state_r = datalab_raster_texture_state_prepare(session->renderer,
                                                                  frame->width,
                                                                  frame->height,
                                                                  &session->raster_texture_state);
        if (state_r.code != CORE_OK) {
            datalab_render_set_failure_diagnostic("render_submit", "raster_texture_prepare", profile, state_r);
            return state_r;
        }
        datalab_raster_texture_state_begin_frame(&session->raster_texture_state);
        run_r = render_sketch_loop(session->window,
                                   session->renderer,
                                   frame,
                                   app_state,
                                   &session->raster_texture_state);
        if (run_r.code != CORE_OK) {
            datalab_render_set_failure_diagnostic("render_submit", "profile_loop:raster", profile, run_r);
        }
        return run_r;
    }
    if (frame->profile == DATALAB_PROFILE_LINE_DIAGNOSTIC) {
        run_r = render_line_diagnostic_loop(session->window, session->renderer, frame, app_state);
        if (run_r.code != CORE_OK) datalab_render_set_failure_diagnostic("render_submit", "profile_loop:line_diagnostic", profile, run_r);
        return run_r;
    }
    run_r = frame->profile == DATALAB_PROFILE_VOLUME
                ? render_volume_loop(session->window, session->renderer, frame, app_state)
                : render_physics_loop(session->window, session->renderer, frame, app_state);
    if (run_r.code != CORE_OK) {
        datalab_render_set_failure_diagnostic("render_submit",
                                              frame->profile == DATALAB_PROFILE_VOLUME ? "profile_loop:volume" : "profile_loop:physics",
                                              profile,
                                              run_r);
    }
    return run_r;
}

CoreResult datalab_render_capture_first_frame(DatalabRenderSession *session,
                                              const DatalabFrame *frame,
                                              DatalabAppState *app_state,
                                              const char *output_path) {
    CoreResult validate_r = datalab_render_validate_frame(frame, app_state);
    CoreResult run_r = core_result_ok();
    DatalabProfile profile = frame ? frame->profile : DATALAB_PROFILE_UNKNOWN;
    if (validate_r.code != CORE_OK) {
        datalab_render_set_failure_diagnostic("visual_artifact", "validate_frame", profile, validate_r);
        return validate_r;
    }
    if (!session || !session->window || !session->renderer) {
        run_r = (CoreResult){ CORE_ERR_INVALID_ARG, "render session is not open" };
        datalab_render_set_failure_diagnostic("visual_artifact", "session_state", profile, run_r);
        return run_r;
    }
    run_r = datalab_render_submit_first_frame(session, frame, app_state);
    if (run_r.code != CORE_OK) {
        datalab_render_set_failure_diagnostic("visual_artifact", "render_first_frame", profile, run_r);
        return run_r;
    }
    run_r = datalab_render_capture_surface(session->renderer, output_path);
    if (run_r.code != CORE_OK) {
        datalab_render_set_failure_diagnostic("visual_artifact", "capture_surface", profile, run_r);
        return run_r;
    }
    return core_result_ok();
}
