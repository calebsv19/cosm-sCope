#ifndef DATALAB_RENDER_VIEW_INTERNAL_H
#define DATALAB_RENDER_VIEW_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "core_base.h"
#include "datalab/datalab_app_main.h"
#include "kit_viz.h"

typedef enum DatalabInputRouteTargetPolicy {
    DATALAB_INPUT_ROUTE_TARGET_FALLBACK = 0,
    DATALAB_INPUT_ROUTE_TARGET_GLOBAL = 1
} DatalabInputRouteTargetPolicy;

typedef enum DatalabInputInvalidateReasonBits {
    DATALAB_INPUT_INVALIDATE_REASON_QUIT = 1u << 0,
    DATALAB_INPUT_INVALIDATE_REASON_KEYBOARD = 1u << 1,
    DATALAB_INPUT_INVALIDATE_REASON_OTHER = 1u << 2
} DatalabInputInvalidateReasonBits;

typedef struct DatalabInputEventRaw {
    uint32_t sdl_event_count;
    uint32_t quit_event_count;
    uint32_t key_event_count;
    uint32_t other_event_count;
    uint8_t quit_requested;
} DatalabInputEventRaw;

typedef struct DatalabInputEventNormalized {
    uint8_t has_quit_action;
    uint8_t has_keyboard_action;
    uint8_t has_other_action;
    uint32_t action_count;
} DatalabInputEventNormalized;

typedef struct DatalabInputRouteResult {
    uint8_t consumed;
    DatalabInputRouteTargetPolicy target_policy;
    uint32_t routed_global_count;
    uint32_t routed_fallback_count;
} DatalabInputRouteResult;

typedef struct DatalabInputInvalidationResult {
    uint8_t full_invalidate;
    uint32_t invalidation_reason_bits;
    uint32_t target_invalidation_count;
    uint32_t full_invalidation_count;
} DatalabInputInvalidationResult;

typedef struct DatalabInputFrame {
    DatalabInputEventRaw raw;
    DatalabInputRouteResult route;
    DatalabInputInvalidationResult invalidation;
} DatalabInputFrame;

typedef struct DatalabInputDiagTotals {
    uint64_t frame_count;
    uint64_t event_count_total;
    uint64_t routed_global_total;
    uint64_t routed_fallback_total;
    uint64_t invalidation_reason_bits_total;
} DatalabInputDiagTotals;

typedef struct DatalabLoopWaitPolicyInput {
    uint8_t high_intensity_mode;
    uint8_t interaction_active;
    uint8_t background_busy;
    uint8_t resize_pending;
} DatalabLoopWaitPolicyInput;

typedef struct DatalabWorkspaceAuthoringAdapterResult {
    uint8_t consumed;
    uint8_t entered_authoring;
} DatalabWorkspaceAuthoringAdapterResult;

typedef struct DatalabRenderDeriveFrame {
    char title[256];
} DatalabRenderDeriveFrame;

typedef struct DatalabRenderSubmitOutcome {
    CoreResult result;
    uint8_t presented;
} DatalabRenderSubmitOutcome;

typedef struct DatalabRenderDiagTotals {
    uint64_t frame_count;
    uint64_t submit_count;
    uint64_t present_count;
} DatalabRenderDiagTotals;

typedef struct DatalabPhysicsRenderDeriveFrame {
    DatalabRenderDeriveFrame common;
    const uint8_t *pixels;
    SDL_Rect dst;
    uint8_t draw_vectors;
} DatalabPhysicsRenderDeriveFrame;

typedef struct DatalabSketchRenderDeriveFrame {
    DatalabRenderDeriveFrame common;
    SDL_Rect dst;
} DatalabSketchRenderDeriveFrame;

int datalab_ir1_diag_enabled(void);
int datalab_rs1_diag_enabled(void);
int datalab_loop_compute_wait_timeout_ms(const DatalabLoopWaitPolicyInput *input);
void datalab_loop_diag_tick(double frame_elapsed_sec,
                            uint32_t wait_blocked_ms,
                            uint32_t wait_call_count);

void datalab_input_frame_begin(DatalabInputFrame *frame);
void datalab_input_apply_event(DatalabInputFrame *frame, const SDL_Event *event);
void datalab_workspace_authoring_route_keydown(const SDL_KeyboardEvent *key,
                                               DatalabAppState *app_state,
                                               DatalabWorkspaceAuthoringAdapterResult *outcome);
int datalab_workspace_authoring_route_mouse_event(const SDL_Event *event, DatalabAppState *app_state);
CoreResult datalab_workspace_authoring_dispatch_action(DatalabAppState *app_state, const char *action_id);

void datalab_sync_text_zoom(const DatalabAppState *app_state);
void datalab_set_text_zoom_step(int step);
int datalab_scaled_px(float px);
int lane_name_eq(const char *a, const char *b);
void make_title(const DatalabFrame *frame, const DatalabAppState *state, char *title, size_t title_cap);
void calc_fit_rect(int ww, int wh, uint32_t fw, uint32_t fh, SDL_Rect *out_rect);
float clamp_unit(float v);
void draw_text_5x7(SDL_Renderer *renderer,
                   int x,
                   int y,
                   const char *text,
                   int scale,
                   uint8_t r,
                   uint8_t g,
                   uint8_t b,
                   uint8_t a);
void draw_text_5x7_clipped(SDL_Renderer *renderer,
                           const SDL_Rect *clip_rect,
                           int x,
                           int y,
                           const char *text,
                           int scale,
                           uint8_t r,
                           uint8_t g,
                           uint8_t b,
                           uint8_t a);
int datalab_measure_text(int scale,
                         const char *text,
                         int *out_width,
                         int *out_height);
int datalab_text_line_height(int scale);
void render_daw_frame(SDL_Renderer *renderer, const DatalabFrame *frame, const DatalabAppState *app_state);

void datalab_draw_session_controls(SDL_Renderer *renderer, const DatalabAppState *app_state);
void datalab_render_derive_frame(const DatalabFrame *frame,
                                 const DatalabAppState *app_state,
                                 DatalabRenderDeriveFrame *out_derive);
void datalab_physics_render_derive_frame(SDL_Renderer *renderer,
                                         const DatalabFrame *frame,
                                         const DatalabAppState *app_state,
                                         const uint8_t *density_rgba,
                                         const uint8_t *speed_rgba,
                                         DatalabPhysicsRenderDeriveFrame *out_derive);
void datalab_sketch_render_derive_frame(SDL_Renderer *renderer,
                                        const DatalabFrame *frame,
                                        const DatalabAppState *app_state,
                                        DatalabSketchRenderDeriveFrame *out_derive);
void datalab_physics_render_submit_frame(SDL_Window *window,
                                         SDL_Renderer *renderer,
                                         SDL_Texture *texture,
                                         const DatalabFrame *frame,
                                         const DatalabAppState *app_state,
                                         KitVizVecSegment *segments,
                                         size_t sample_count,
                                         const DatalabPhysicsRenderDeriveFrame *derive,
                                         DatalabRenderSubmitOutcome *outcome);
void datalab_sketch_render_submit_frame(SDL_Window *window,
                                        SDL_Renderer *renderer,
                                        SDL_Texture *texture,
                                        const DatalabFrame *frame,
                                        const DatalabAppState *app_state,
                                        const DatalabSketchRenderDeriveFrame *derive,
                                        DatalabRenderSubmitOutcome *outcome);
void datalab_daw_render_submit_frame(SDL_Window *window,
                                     SDL_Renderer *renderer,
                                     const DatalabFrame *frame,
                                     const DatalabAppState *app_state,
                                     const DatalabRenderDeriveFrame *derive,
                                     DatalabRenderSubmitOutcome *outcome);
void datalab_trace_render_submit_frame(SDL_Window *window,
                                       SDL_Renderer *renderer,
                                       const DatalabFrame *frame,
                                       DatalabAppState *app_state,
                                       const DatalabRenderDeriveFrame *derive,
                                       DatalabRenderSubmitOutcome *outcome);
void datalab_workspace_authoring_submit_takeover(SDL_Window *window,
                                                 SDL_Renderer *renderer,
                                                 const DatalabFrame *frame,
                                                 const DatalabAppState *app_state,
                                                 const char *title,
                                                 DatalabRenderSubmitOutcome *outcome);
void datalab_rs1_diag_note(const char *lane,
                           DatalabRenderDiagTotals *totals,
                           const DatalabRenderSubmitOutcome *submit);

CoreResult render_physics_loop(SDL_Window *window,
                               SDL_Renderer *renderer,
                               const DatalabFrame *frame,
                               DatalabAppState *app_state);
CoreResult render_sketch_loop(SDL_Window *window,
                              SDL_Renderer *renderer,
                              const DatalabFrame *frame,
                              DatalabAppState *app_state);
CoreResult render_daw_loop(SDL_Window *window,
                           SDL_Renderer *renderer,
                           const DatalabFrame *frame,
                           DatalabAppState *app_state);
CoreResult render_trace_loop(SDL_Window *window,
                             SDL_Renderer *renderer,
                             const DatalabFrame *frame,
                             DatalabAppState *app_state);

#endif
