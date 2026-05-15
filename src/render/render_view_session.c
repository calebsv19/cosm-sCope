#include "render/render_view.h"
#include "render/render_view_internal.h"

#include <SDL2/SDL.h>

#include <string.h>

struct DatalabRenderSession {
    SDL_Window *window;
    SDL_Renderer *renderer;
    DatalabRasterTextureState raster_texture_state;
    int sdl_owner;
};

static CoreResult datalab_render_validate_frame(const DatalabFrame *frame, const DatalabAppState *app_state) {
    if (!frame || !app_state) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    if (frame->profile == DATALAB_PROFILE_PHYSICS) {
        if (!frame->density || !frame->velx || !frame->vely || frame->width == 0 || frame->height == 0) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid physics frame" };
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
    return (CoreResult){ CORE_ERR_INVALID_ARG, "unknown frame profile" };
}

CoreResult datalab_render_session_open(DatalabRenderSession **out_session) {
    DatalabRenderSession *session = NULL;
    const uint32_t video_mask = SDL_INIT_VIDEO;
    const uint32_t window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (!out_session) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid render session request" };
    }
    *out_session = NULL;
    session = (DatalabRenderSession *)SDL_calloc(1, sizeof(*session));
    if (!session) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "failed to allocate render session" };
    }
    if ((SDL_WasInit(video_mask) & video_mask) == 0u) {
        if (SDL_Init(video_mask) != 0) {
            SDL_free(session);
            return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
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
        datalab_render_session_close(session);
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    session->renderer = SDL_CreateRenderer(session->window,
                                           -1,
                                           SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!session->renderer) {
        datalab_render_session_close(session);
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
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
    if (validate_r.code != CORE_OK) {
        return validate_r;
    }
    if (!session || !session->window || !session->renderer) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "render session is not open" };
    }
    if (frame->profile == DATALAB_PROFILE_DAW) {
        return render_daw_loop(session->window, session->renderer, frame, app_state);
    }
    if (frame->profile == DATALAB_PROFILE_TRACE) {
        return render_trace_loop(session->window, session->renderer, frame, app_state);
    }
    if (frame->profile == DATALAB_PROFILE_SKETCH || frame->profile == DATALAB_PROFILE_IMAGE) {
        CoreResult state_r = datalab_raster_texture_state_prepare(session->renderer,
                                                                  frame->width,
                                                                  frame->height,
                                                                  &session->raster_texture_state);
        if (state_r.code != CORE_OK) {
            return state_r;
        }
        datalab_raster_texture_state_begin_frame(&session->raster_texture_state);
        return render_sketch_loop(session->window,
                                  session->renderer,
                                  frame,
                                  app_state,
                                  &session->raster_texture_state);
    }
    return render_physics_loop(session->window, session->renderer, frame, app_state);
}
