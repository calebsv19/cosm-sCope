#include "render/render_view_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ui/input.h"

typedef struct DatalabPhysicsLoopContext {
    SDL_Texture *texture;
    uint8_t *density_rgba;
    uint8_t *speed_rgba;
    KitVizVecSegment *segments;
    size_t sample_count;
} DatalabPhysicsLoopContext;

typedef struct DatalabSketchLoopContext {
    DatalabRasterTextureState *texture_state;
} DatalabSketchLoopContext;

static CoreResult datalab_loop_render_step_physics(SDL_Window *window,
                                                   SDL_Renderer *renderer,
                                                   const DatalabFrame *frame,
                                                   DatalabAppState *app_state,
                                                   void *lane_ctx,
                                                   DatalabRenderSubmitOutcome *out_submit) {
    DatalabPhysicsLoopContext *ctx = (DatalabPhysicsLoopContext *)lane_ctx;
    DatalabPhysicsRenderDeriveFrame render_derive;
    if (!ctx || !out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid physics loop context" };
    }
    datalab_physics_render_derive_frame(renderer,
                                        frame,
                                        app_state,
                                        ctx->density_rgba,
                                        ctx->speed_rgba,
                                        &render_derive);
    datalab_physics_render_submit_frame(window,
                                        renderer,
                                        ctx->texture,
                                        frame,
                                        app_state,
                                        ctx->segments,
                                        ctx->sample_count,
                                        &render_derive,
                                        out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_daw(SDL_Window *window,
                                               SDL_Renderer *renderer,
                                               const DatalabFrame *frame,
                                               DatalabAppState *app_state,
                                               void *lane_ctx,
                                               DatalabRenderSubmitOutcome *out_submit) {
    DatalabRenderDeriveFrame render_derive;
    (void)lane_ctx;
    if (!out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid daw loop outcome" };
    }
    datalab_render_derive_frame(frame, app_state, &render_derive);
    datalab_daw_render_submit_frame(window, renderer, frame, app_state, &render_derive, out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_trace(SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 const DatalabFrame *frame,
                                                 DatalabAppState *app_state,
                                                 void *lane_ctx,
                                                 DatalabRenderSubmitOutcome *out_submit) {
    DatalabRenderDeriveFrame render_derive;
    (void)lane_ctx;
    if (!out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid trace loop outcome" };
    }
    datalab_render_derive_frame(frame, app_state, &render_derive);
    datalab_trace_render_submit_frame(window, renderer, frame, app_state, &render_derive, out_submit);
    return out_submit->result;
}

static CoreResult datalab_loop_render_step_sketch(SDL_Window *window,
                                                  SDL_Renderer *renderer,
                                                  const DatalabFrame *frame,
                                                  DatalabAppState *app_state,
                                                  void *lane_ctx,
                                                  DatalabRenderSubmitOutcome *out_submit) {
    DatalabSketchLoopContext *ctx = (DatalabSketchLoopContext *)lane_ctx;
    DatalabSketchRenderDeriveFrame render_derive;
    if (!ctx || !out_submit) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid sketch loop context" };
    }
    datalab_sketch_render_derive_frame(renderer, frame, app_state, &render_derive);
    datalab_sketch_render_submit_frame(window,
                                       renderer,
                                       ctx->texture_state,
                                       frame,
                                       app_state,
                                       &render_derive,
                                       out_submit);
    return out_submit->result;
}

CoreResult render_physics_loop(SDL_Window *window,
                               SDL_Renderer *renderer,
                               const DatalabFrame *frame,
                               DatalabAppState *app_state) {
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             (int)frame->width,
                                             (int)frame->height);
    const size_t sample_count = (size_t)frame->width * (size_t)frame->height;
    const size_t rgba_size = sample_count * 4u;
    uint8_t *density_rgba = NULL;
    uint8_t *speed_rgba = NULL;
    float *speed = NULL;
    KitVizVecSegment *segments = NULL;
    DatalabPhysicsLoopContext physics_ctx;
    DatalabLoopProfileOps ops;
    CoreResult loop_result = core_result_ok();
    if (!texture) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }

    density_rgba = (uint8_t *)core_alloc(rgba_size);
    speed_rgba = (uint8_t *)core_alloc(rgba_size);
    speed = (float *)core_alloc(sample_count * sizeof(float));
    segments = (KitVizVecSegment *)core_alloc(sample_count * sizeof(KitVizVecSegment));
    if (!density_rgba || !speed_rgba || !speed || !segments) {
        loop_result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }

    {
        KitVizFieldStats dens_stats;
        CoreResult rs = kit_viz_compute_field_stats(frame->density, frame->width, frame->height, &dens_stats);
        if (rs.code != CORE_OK) {
            loop_result = rs;
            goto cleanup;
        }
        for (size_t i = 0; i < sample_count; ++i) {
            const float x = frame->velx[i];
            const float y = frame->vely[i];
            speed[i] = sqrtf((x * x) + (y * y));
        }
        {
            KitVizFieldStats speed_stats;
            rs = kit_viz_compute_field_stats(speed, frame->width, frame->height, &speed_stats);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
            rs = kit_viz_build_heatmap_rgba(frame->density,
                                            frame->width,
                                            frame->height,
                                            dens_stats.min_value,
                                            dens_stats.max_value,
                                            KIT_VIZ_COLORMAP_HEAT,
                                            density_rgba,
                                            rgba_size);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
            rs = kit_viz_build_heatmap_rgba(speed,
                                            frame->width,
                                            frame->height,
                                            speed_stats.min_value,
                                            speed_stats.max_value,
                                            KIT_VIZ_COLORMAP_HEAT,
                                            speed_rgba,
                                            rgba_size);
            if (rs.code != CORE_OK) {
                loop_result = rs;
                goto cleanup;
            }
        }
    }

    physics_ctx.texture = texture;
    physics_ctx.density_rgba = density_rgba;
    physics_ctx.speed_rgba = speed_rgba;
    physics_ctx.segments = segments;
    physics_ctx.sample_count = sample_count;

    ops.lane_tag = "physics";
    ops.lane_ctx = &physics_ctx;
    ops.render_step = datalab_loop_render_step_physics;
    loop_result = datalab_loop_run_profile(window, renderer, frame, app_state, &ops);

cleanup:
    core_free(density_rgba);
    core_free(speed_rgba);
    core_free(speed);
    core_free(segments);
    SDL_DestroyTexture(texture);
    return loop_result;
}

CoreResult render_daw_loop(SDL_Window *window,
                           SDL_Renderer *renderer,
                           const DatalabFrame *frame,
                           DatalabAppState *app_state) {
    const DatalabLoopProfileOps ops = {
        .lane_tag = "daw",
        .lane_ctx = NULL,
        .render_step = datalab_loop_render_step_daw
    };
    return datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
}

CoreResult render_sketch_loop(SDL_Window *window,
                              SDL_Renderer *renderer,
                              const DatalabFrame *frame,
                              DatalabAppState *app_state,
                              DatalabRasterTextureState *texture_state) {
    DatalabSketchLoopContext sketch_ctx;
    DatalabLoopProfileOps ops;
    CoreResult loop_result = core_result_ok();
    if (!texture_state) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "missing sketch texture state" };
    }
    memset(&sketch_ctx, 0, sizeof(sketch_ctx));
    sketch_ctx.texture_state = texture_state;
    ops.lane_tag = "sketch";
    ops.lane_ctx = &sketch_ctx;
    ops.render_step = datalab_loop_render_step_sketch;
    loop_result = datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
    return loop_result;
}

CoreResult render_trace_loop(SDL_Window *window,
                             SDL_Renderer *renderer,
                             const DatalabFrame *frame,
                             DatalabAppState *app_state) {
    const DatalabLoopProfileOps ops = {
        .lane_tag = "trace",
        .lane_ctx = NULL,
        .render_step = datalab_loop_render_step_trace
    };
    return datalab_loop_run_profile(window, renderer, frame, app_state, &ops);
}
