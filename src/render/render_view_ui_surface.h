#ifndef DATALAB_RENDER_VIEW_UI_SURFACE_H
#define DATALAB_RENDER_VIEW_UI_SURFACE_H
#include "render/render_view_ui_buttons.h"
#include "kit_ui_surface.h"
enum { DATALAB_UI_TOP=1, DATALAB_UI_FONT, DATALAB_UI_PLAYBACK,
       DATALAB_UI_RECENT_HEADER, DATALAB_UI_RECENT_ROOT };
extern KitUiSurface g_datalab_button_surface;
void datalab_ui_surface_reset(void);
int datalab_ui_surface_viewport_matches(int width, int height);
uint64_t datalab_ui_path_key(const char *path);
void datalab_ui_surface_add(uint32_t domain, uint64_t value, SDL_Rect rect, int enabled);
CoreResult datalab_ui_surface_collect(const DatalabAppState *state, int width, int height);
KitUiButtonState datalab_ui_surface_button_state(uint32_t domain, uint64_t value,
    const SDL_Rect *rect, int enabled, int selected);
void datalab_ui_surface_draw_button(SDL_Renderer *renderer, const SDL_Rect *rect,
    const char *label, uint32_t domain, uint64_t value, int enabled, int selected,
    const DatalabAuthoringThemePalette *palette);
void datalab_ui_surface_draw_focus(SDL_Renderer *renderer, const DatalabAppState *state);
void datalab_playback_hud_collect_buttons(const DatalabAppState *state);
void datalab_playback_hud_apply_button(DatalabAppState *state, uint64_t value);
void datalab_recent_collect_buttons(const DatalabAppState *state);
void datalab_recent_apply_button(DatalabAppState *state, uint32_t domain, uint64_t value);
int datalab_overlay_apply_top_button(KitWorkspaceAuthoringOverlayButtonId id, DatalabAppState *state);
#endif
