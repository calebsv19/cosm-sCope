#include "app/app_state.h"

#include <SDL2/SDL.h>

/* DataLab owns draft values, theme preview, and runtime policy. This adapter
 * owns only shared session transitions and the fail-closed runtime gate. */

void datalab_workspace_authoring_draft_begin(DatalabAppState *state);
void datalab_workspace_authoring_draft_apply(DatalabAppState *state);
int datalab_workspace_authoring_draft_cancel(DatalabAppState *state);

static CoreWorkspaceAuthoringSessionHookResult datalab_authoring_session_begin(void *context) {
    DatalabAppState *state = context;
    if (!state) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    datalab_workspace_authoring_draft_begin(state);
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult datalab_authoring_session_validate(void *context) {
    return context ? CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK
                   : CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
}

static CoreWorkspaceAuthoringSessionHookResult datalab_authoring_session_apply(void *context) {
    DatalabAppState *state = context;
    if (!state) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    datalab_workspace_authoring_draft_apply(state);
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult datalab_authoring_session_cancel(void *context) {
    DatalabAppState *state = context;
    if (!state) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    (void)datalab_workspace_authoring_draft_cancel(state);
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult datalab_authoring_session_resume(void *context) {
    DatalabAppState *state = context;
    if (!state) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    state->workspace_authoring_stub_active = 0;
    state->workspace_authoring_overlay_mode = DATALAB_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state->workspace_authoring_custom_theme_popup_open = 0u;
    state->workspace_authoring_entry_chord_mask = 0u;
    /* Do not let authoring time count as playback time when runtime resumes. */
    state->playback_last_advance_ticks = SDL_GetTicks();
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult datalab_authoring_session_recover(void *context) {
    return datalab_authoring_session_cancel(context);
}

static const CoreWorkspaceAuthoringSessionHooks k_datalab_authoring_session_hooks = {
    datalab_authoring_session_begin,
    datalab_authoring_session_validate,
    datalab_authoring_session_apply,
    datalab_authoring_session_cancel,
    datalab_authoring_session_resume,
    datalab_authoring_session_recover
};

void datalab_workspace_authoring_session_init(DatalabAppState *state) {
    if (!state) return;
    core_workspace_authoring_session_init(
        &state->workspace_authoring_session,
        CORE_WORKSPACE_AUTHORING_CAP_FONT_THEME_DRAFT |
            CORE_WORKSPACE_AUTHORING_CAP_LAYOUT_DRAFT |
            CORE_WORKSPACE_AUTHORING_CAP_SAFE_RUNTIME_GATE,
        state,
        &k_datalab_authoring_session_hooks);
}

void datalab_workspace_authoring_begin_takeover(DatalabAppState *state) {
    if (state) (void)core_workspace_authoring_session_enter(&state->workspace_authoring_session);
}

void datalab_workspace_authoring_apply_takeover(DatalabAppState *state) {
    if (state) (void)core_workspace_authoring_session_apply(&state->workspace_authoring_session);
}

int datalab_workspace_authoring_cancel_and_exit(DatalabAppState *state) {
    int restored_text_zoom;
    if (!state) return 0;
    restored_text_zoom = state->workspace_authoring_pending_stub ? 1 : 0;
    (void)core_workspace_authoring_session_cancel(&state->workspace_authoring_session);
    return restored_text_zoom;
}

int datalab_workspace_authoring_runtime_mutation_allowed(const DatalabAppState *state) {
    return state && !state->workspace_authoring_stub_active &&
           core_workspace_authoring_session_runtime_mutation_allowed(&state->workspace_authoring_session);
}

void datalab_workspace_authoring_recover_failed_safe(DatalabAppState *state) {
    if (state) (void)core_workspace_authoring_session_recover(&state->workspace_authoring_session);
}

void datalab_workspace_authoring_shutdown(DatalabAppState *state) {
    if (!state) return;
    if (state->workspace_authoring_session.state == CORE_WORKSPACE_AUTHORING_SESSION_FAILED_SAFE) {
        datalab_workspace_authoring_recover_failed_safe(state);
    } else if (core_workspace_authoring_session_authoring_active(&state->workspace_authoring_session)) {
        (void)core_workspace_authoring_session_cancel(&state->workspace_authoring_session);
    }
}
