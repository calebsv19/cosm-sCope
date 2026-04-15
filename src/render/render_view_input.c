#include "render/render_view_internal.h"

#include <string.h>

void datalab_input_frame_begin(DatalabInputFrame *frame) {
    if (!frame) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
}

static void datalab_input_intake(const SDL_Event *event, DatalabInputEventRaw *out_raw) {
    if (!event || !out_raw) {
        return;
    }
    out_raw->sdl_event_count += 1u;
    switch (event->type) {
        case SDL_QUIT:
            out_raw->quit_event_count += 1u;
            out_raw->quit_requested = 1u;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            out_raw->key_event_count += 1u;
            break;
        default:
            out_raw->other_event_count += 1u;
            break;
    }
}

static void datalab_input_normalize(const SDL_Event *event,
                                    DatalabInputEventNormalized *out_normalized) {
    if (!event || !out_normalized) {
        return;
    }
    memset(out_normalized, 0, sizeof(*out_normalized));
    switch (event->type) {
        case SDL_QUIT:
            out_normalized->has_quit_action = 1u;
            out_normalized->action_count = 1u;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            out_normalized->has_keyboard_action = 1u;
            out_normalized->action_count = 1u;
            break;
        default:
            out_normalized->has_other_action = 1u;
            out_normalized->action_count = 1u;
            break;
    }
}

static void datalab_input_route(const DatalabInputEventNormalized *normalized,
                                DatalabInputRouteResult *out_route) {
    if (!normalized || !out_route) {
        return;
    }
    memset(out_route, 0, sizeof(*out_route));
    out_route->target_policy = DATALAB_INPUT_ROUTE_TARGET_FALLBACK;
    if (normalized->has_quit_action || normalized->has_keyboard_action) {
        out_route->target_policy = DATALAB_INPUT_ROUTE_TARGET_GLOBAL;
        out_route->routed_global_count = normalized->action_count;
        out_route->consumed = normalized->action_count > 0u;
        return;
    }
    out_route->routed_fallback_count = normalized->action_count;
    out_route->consumed = normalized->action_count > 0u;
}

static void datalab_input_invalidate(const DatalabInputEventNormalized *normalized,
                                     const DatalabInputRouteResult *route,
                                     DatalabInputInvalidationResult *out_invalidation) {
    if (!normalized || !route || !out_invalidation) {
        return;
    }
    memset(out_invalidation, 0, sizeof(*out_invalidation));
    if (normalized->has_quit_action) {
        out_invalidation->invalidation_reason_bits |= DATALAB_INPUT_INVALIDATE_REASON_QUIT;
        out_invalidation->full_invalidation_count += 1u;
        out_invalidation->full_invalidate = 1u;
    }
    if (normalized->has_keyboard_action) {
        out_invalidation->invalidation_reason_bits |= DATALAB_INPUT_INVALIDATE_REASON_KEYBOARD;
    }
    if (normalized->has_other_action) {
        out_invalidation->invalidation_reason_bits |= DATALAB_INPUT_INVALIDATE_REASON_OTHER;
    }
    out_invalidation->target_invalidation_count += route->routed_global_count;
    out_invalidation->target_invalidation_count += route->routed_fallback_count;
}

void datalab_input_apply_event(DatalabInputFrame *frame, const SDL_Event *event) {
    DatalabInputEventNormalized normalized;
    DatalabInputRouteResult route;
    DatalabInputInvalidationResult invalidation;
    if (!frame || !event) {
        return;
    }
    datalab_input_intake(event, &frame->raw);
    datalab_input_normalize(event, &normalized);
    datalab_input_route(&normalized, &route);
    datalab_input_invalidate(&normalized, &route, &invalidation);

    frame->route.routed_global_count += route.routed_global_count;
    frame->route.routed_fallback_count += route.routed_fallback_count;
    if (route.consumed) {
        frame->route.consumed = 1u;
        frame->route.target_policy = route.target_policy;
    }
    frame->invalidation.invalidation_reason_bits |= invalidation.invalidation_reason_bits;
    frame->invalidation.target_invalidation_count += invalidation.target_invalidation_count;
    frame->invalidation.full_invalidation_count += invalidation.full_invalidation_count;
    if (invalidation.full_invalidate) {
        frame->invalidation.full_invalidate = 1u;
    }
}
