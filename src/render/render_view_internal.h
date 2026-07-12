#ifndef DATALAB_RENDER_VIEW_INTERNAL_H
#define DATALAB_RENDER_VIEW_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "core_base.h"
#include "datalab/datalab_app_main.h"
#include "kit_graph_timeseries.h"
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

typedef struct DatalabLoopBoundarySignals {
    uint8_t sync_input_invalidated;
    uint8_t async_panel_rescan_pending;
    uint8_t async_authoring_pending;
} DatalabLoopBoundarySignals;

enum {
    DATALAB_LOOP_RENDER_HEARTBEAT_MS = 250u,
    DATALAB_LOOP_RENDER_REASON_FORCE = 1u << 0,
    DATALAB_LOOP_RENDER_REASON_INPUT_INVALIDATE = 1u << 1,
    DATALAB_LOOP_RENDER_REASON_ASYNC_PANEL_RESCAN = 1u << 2,
    DATALAB_LOOP_RENDER_REASON_ASYNC_AUTHORING = 1u << 3,
    DATALAB_LOOP_RENDER_REASON_RESIZE = 1u << 4,
    DATALAB_LOOP_RENDER_REASON_HEARTBEAT = 1u << 5
};

typedef struct DatalabWorkspaceAuthoringAdapterResult {
    uint8_t consumed;
    uint8_t entered_authoring;
} DatalabWorkspaceAuthoringAdapterResult;

typedef struct DatalabWorkspaceAuthoringRouteDiagnostic {
    char route[32];
    char action[48];
    char detail[96];
    int result_code;
    uint8_t consumed;
    uint8_t active_before;
    uint8_t active_after;
    uint8_t pending_before;
    uint8_t pending_after;
    DatalabWorkspaceAuthoringOverlayMode overlay_before;
    DatalabWorkspaceAuthoringOverlayMode overlay_after;
    uint64_t sequence;
} DatalabWorkspaceAuthoringRouteDiagnostic;

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

typedef struct DatalabRenderFailureDiagnostic {
    char stage[32];
    char route[48];
    char profile[32];
    char detail[128];
    int result_code;
    uint64_t sequence;
} DatalabRenderFailureDiagnostic;

typedef CoreResult (*DatalabLoopRenderStepFn)(SDL_Window *window,
                                              SDL_Renderer *renderer,
                                              const DatalabFrame *frame,
                                              DatalabAppState *app_state,
                                              void *lane_ctx,
                                              DatalabRenderSubmitOutcome *out_submit);

typedef struct DatalabLoopProfileOps {
    const char *lane_tag;
    void *lane_ctx;
    DatalabLoopRenderStepFn render_step;
} DatalabLoopProfileOps;

typedef struct DatalabPhysicsRenderDeriveFrame {
    DatalabRenderDeriveFrame common;
    const uint8_t *pixels;
    SDL_Rect dst;
    uint8_t draw_vectors;
} DatalabPhysicsRenderDeriveFrame;

typedef struct DatalabSketchRenderDeriveFrame {
    DatalabRenderDeriveFrame common;
    SDL_Rect dst;
    float zoom;
    uint8_t fit_mode;
} DatalabSketchRenderDeriveFrame;

typedef struct DatalabRasterTileCacheEntry {
    SDL_Texture *texture;
    int tile_x_index;
    int tile_y_index;
    int tile_w;
    int tile_h;
    uint64_t stamp;
    uint64_t frame_generation;
    int valid;
} DatalabRasterTileCacheEntry;

typedef struct DatalabRasterTextureState {
    SDL_Texture *full_texture;
    SDL_Texture *tile_texture;
    DatalabRasterTileCacheEntry *cache_entries;
    int use_tiled;
    int tile_edge;
    int cache_capacity;
    int prefetch_radius;
    int max_texture_width;
    int max_texture_height;
    uint32_t content_width;
    uint32_t content_height;
    uint64_t cache_stamp;
    uint64_t frame_generation;
} DatalabRasterTextureState;

#define DATALAB_PANEL_MAX_FILES 160

typedef struct DatalabPackPanelCache {
    char scanned_root[DATALAB_APP_PATH_CAP];
    char files[DATALAB_PANEL_MAX_FILES][DATALAB_APP_PATH_CAP];
    size_t file_count;
    uint32_t last_scan_ticks;
    char status[160];
} DatalabPackPanelCache;

typedef struct DatalabSupportedFileScanResult {
    size_t file_count;
    int invalid_request;
    int root_unavailable;
} DatalabSupportedFileScanResult;

int datalab_ir1_diag_enabled(void);
int datalab_rs1_diag_enabled(void);
int datalab_loop_compute_wait_timeout_ms(const DatalabLoopWaitPolicyInput *input);
void datalab_loop_update_wait_policy_input(DatalabLoopWaitPolicyInput *policy,
                                           const DatalabInputFrame *input_frame,
                                           const DatalabAppState *app_state,
                                           int panel_rescan_pending,
                                           int resize_pending);
uint32_t datalab_loop_compute_render_reason_bits(const DatalabLoopBoundarySignals *signals,
                                                 int resize_pending,
                                                 uint32_t last_present_ticks,
                                                 uint32_t now_ticks);
void datalab_session_controls_tick(DatalabAppState *app_state);
size_t datalab_panel_find_active_index(const DatalabPackPanelCache *cache, const char *active_path);
DatalabSupportedFileScanResult datalab_scan_supported_files(const char *root,
                                                            char files[][DATALAB_APP_PATH_CAP],
                                                            size_t max_files);
void datalab_format_supported_file_count_status(size_t file_count, char *status, size_t status_cap);
void datalab_format_supported_file_scan_status(const DatalabSupportedFileScanResult *scan,
                                               const char *root,
                                               const char *recovery_hint,
                                               char *status,
                                               size_t status_cap);
int datalab_render_point_in_rect(const SDL_Rect *rect, int x, int y);
int datalab_render_map_window_to_renderer_point(SDL_Window *window,
                                                SDL_Renderer *renderer,
                                                int window_x,
                                                int window_y,
                                                int *out_render_x,
                                                int *out_render_y);
void datalab_panel_apply_state(DatalabAppState *app_state,
                               DatalabPackPanelCache *cache,
                               const char *root,
                               int rescanned,
                               uint32_t now_ticks);
void datalab_loop_diag_tick(double frame_elapsed_sec,
                            uint32_t wait_blocked_ms,
                            uint32_t wait_call_count);
int datalab_session_controls_mouse_enabled(const DatalabAppState *app_state);
size_t datalab_session_controls_file_count(void);
const char *datalab_session_controls_selected_file_name(const DatalabAppState *app_state);
void datalab_draw_playback_hud(SDL_Renderer *renderer, const DatalabAppState *app_state);
CoreResult datalab_trace_graph_draw_shared(SDL_Renderer *renderer,
                                           int frame_width,
                                           int frame_height,
                                           const SDL_Rect *band,
                                           const KitGraphTsSeries *series,
                                           float zoom_factor,
                                           int inspect_active,
                                           float inspect_x,
                                           float inspect_y,
                                           KitGraphTsHover *out_hover);
const DatalabRenderFailureDiagnostic *datalab_render_last_failure_diagnostic(void);
void datalab_render_clear_failure_diagnostic(void);
const char *datalab_render_last_failure_summary(void);

void datalab_input_frame_begin(DatalabInputFrame *frame);
void datalab_input_apply_event(DatalabInputFrame *frame, const SDL_Event *event);
void datalab_workspace_authoring_route_keydown(const SDL_KeyboardEvent *key,
                                               DatalabAppState *app_state,
                                               DatalabWorkspaceAuthoringAdapterResult *outcome);
int datalab_workspace_authoring_route_mouse_event(const SDL_Event *event, DatalabAppState *app_state);
int datalab_session_controls_route_mouse_event(SDL_Window *window,
                                               SDL_Renderer *renderer,
                                               const SDL_Event *event,
                                               DatalabAppState *app_state);
int datalab_playback_hud_route_mouse_event(SDL_Window *window,
                                           SDL_Renderer *renderer,
                                           const SDL_Event *event,
                                           DatalabAppState *app_state);
CoreResult datalab_workspace_authoring_dispatch_action(DatalabAppState *app_state, const char *action_id);
CoreResult datalab_workspace_authoring_dispatch_action_for_route(DatalabAppState *app_state,
                                                                 const char *action_id,
                                                                 const char *route);
const DatalabWorkspaceAuthoringRouteDiagnostic *datalab_workspace_authoring_last_route_diagnostic(void);
void datalab_workspace_authoring_clear_route_diagnostic(void);

void datalab_sync_text_zoom(const DatalabAppState *app_state);
void datalab_set_text_zoom_step(int step);
int datalab_scaled_px(float px);
int datalab_text_renderer_init(void);
void datalab_text_renderer_shutdown(void);
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

void datalab_draw_recent_input_root_header(SDL_Renderer *renderer, const DatalabAppState *app_state);
void datalab_draw_session_controls(SDL_Renderer *renderer, const DatalabAppState *app_state);
CoreResult datalab_loop_run_profile(SDL_Window *window,
                                    SDL_Renderer *renderer,
                                    const DatalabFrame *frame,
                                    DatalabAppState *app_state,
                                    const DatalabLoopProfileOps *ops);
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
                                        DatalabAppState *app_state,
                                        DatalabSketchRenderDeriveFrame *out_derive);
void datalab_raster_viewport_derive_frame(SDL_Renderer *renderer,
                                          const DatalabFrame *frame,
                                          DatalabAppState *app_state,
                                          DatalabSketchRenderDeriveFrame *out_derive);
CoreResult datalab_raster_texture_state_init(SDL_Renderer *renderer,
                                             uint32_t content_width,
                                             uint32_t content_height,
                                             DatalabRasterTextureState *state);
CoreResult datalab_raster_texture_state_prepare(SDL_Renderer *renderer,
                                                uint32_t content_width,
                                                uint32_t content_height,
                                                DatalabRasterTextureState *state);
void datalab_raster_texture_state_begin_frame(DatalabRasterTextureState *state);
void datalab_raster_texture_state_destroy(DatalabRasterTextureState *state);
CoreResult datalab_raster_render_frame(SDL_Renderer *renderer,
                                       const DatalabFrame *frame,
                                       const DatalabSketchRenderDeriveFrame *derive,
                                       DatalabRasterTextureState *state);
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
                                        DatalabRasterTextureState *texture_state,
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
CoreResult render_volume_loop(SDL_Window *window,
                              SDL_Renderer *renderer,
                              const DatalabFrame *frame,
                              DatalabAppState *app_state);
CoreResult render_sketch_loop(SDL_Window *window,
                              SDL_Renderer *renderer,
                              const DatalabFrame *frame,
                              DatalabAppState *app_state,
                              DatalabRasterTextureState *texture_state);
CoreResult render_daw_loop(SDL_Window *window,
                           SDL_Renderer *renderer,
                           const DatalabFrame *frame,
                           DatalabAppState *app_state);
CoreResult render_trace_loop(SDL_Window *window,
                             SDL_Renderer *renderer,
                             const DatalabFrame *frame,
                             DatalabAppState *app_state);
CoreResult render_line_diagnostic_loop(SDL_Window *window,
                                      SDL_Renderer *renderer,
                                      const DatalabFrame *frame,
                                      DatalabAppState *app_state);
CoreResult datalab_line_diagnostic_submit_frame(SDL_Window *window,
                                                SDL_Renderer *renderer,
                                                const DatalabFrame *frame,
                                                DatalabRenderSubmitOutcome *outcome);

#endif
