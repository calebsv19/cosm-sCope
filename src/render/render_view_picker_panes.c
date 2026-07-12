#include "render_view_picker_panes.h"

#include "kit_ui_sdl.h"

static int datalab_picker_scroll_max_offset(const DatalabPickerScrollState *state) {
    int max_offset = state->content_rows - state->visible_rows;
    return max_offset > 0 ? max_offset : 0;
}

void datalab_picker_scroll_configure(DatalabPickerScrollState *state,
                                     SDL_Rect viewport,
                                     int content_rows,
                                     int visible_rows,
                                     int row_height) {
    int max_offset = 0;
    KitUiSdlScrollbarLayout layout = {0};
    if (!state) return;
    state->viewport = viewport;
    state->content_rows = content_rows > 0 ? content_rows : 0;
    state->visible_rows = visible_rows > 0 ? visible_rows : 1;
    max_offset = datalab_picker_scroll_max_offset(state);
    if (state->offset_rows < 0) state->offset_rows = 0;
    if (state->offset_rows > max_offset) state->offset_rows = max_offset;
    if (row_height < 1) row_height = 1;
    kit_ui_sdl_scrollbar_layout(&viewport,
                                state->content_rows * row_height,
                                state->offset_rows * row_height,
                                &layout);
    state->track = layout.track;
    state->thumb = layout.thumb;
}

void datalab_picker_scroll_by(DatalabPickerScrollState *state, int delta_rows) {
    int max_offset = 0;
    if (!state) return;
    max_offset = datalab_picker_scroll_max_offset(state);
    state->offset_rows += delta_rows;
    if (state->offset_rows < 0) state->offset_rows = 0;
    if (state->offset_rows > max_offset) state->offset_rows = max_offset;
}

int datalab_picker_scroll_begin_drag(DatalabPickerScrollState *state, int x, int y) {
    if (!state || x < state->thumb.x || x >= state->thumb.x + state->thumb.w ||
        y < state->thumb.y || y >= state->thumb.y + state->thumb.h) return 0;
    state->drag_active = 1;
    state->drag_origin_y = y;
    state->drag_origin_offset = state->offset_rows;
    return 1;
}

void datalab_picker_scroll_drag_to(DatalabPickerScrollState *state, int y) {
    int max_offset = 0;
    int travel = 0;
    if (!state || !state->drag_active) return;
    max_offset = datalab_picker_scroll_max_offset(state);
    travel = state->track.h - state->thumb.h;
    if (max_offset <= 0 || travel <= 0) return;
    state->offset_rows = state->drag_origin_offset + ((y - state->drag_origin_y) * max_offset) / travel;
    if (state->offset_rows < 0) state->offset_rows = 0;
    if (state->offset_rows > max_offset) state->offset_rows = max_offset;
}

void datalab_picker_scroll_end_drag(DatalabPickerScrollState *state) {
    if (state) state->drag_active = 0;
}

void datalab_picker_scroll_draw(SDL_Renderer *renderer,
                                const DatalabPickerScrollState *state,
                                const DatalabPickerThemePalette *palette) {
    if (!renderer || !state || !palette || state->content_rows <= state->visible_rows) return;
    kit_ui_sdl_draw_scrollbar(renderer,
                              &(KitUiSdlScrollbarLayout){state->track, state->thumb, 1},
                              (KitRenderColor){palette->top_fill_r, palette->top_fill_g, palette->top_fill_b, 230},
                              (KitRenderColor){palette->selected_border_r, palette->selected_border_g,
                                               palette->selected_border_b,
                                               (uint8_t)(state->drag_active ? 255 : 210)});
}

void datalab_picker_scroll_draw_selection_marker(SDL_Renderer *renderer,
                                                 const DatalabPickerScrollState *state,
                                                 int selected_row,
                                                 const DatalabPickerThemePalette *palette) {
    SDL_Rect marker;
    int y = 0;
    if (!renderer || !state || !palette || state->content_rows <= state->visible_rows ||
        selected_row < 0 || selected_row >= state->content_rows || state->track.h <= 0) return;
    y = state->track.y + ((selected_row * (state->track.h - 1)) / (state->content_rows - 1));
    marker = (SDL_Rect){state->track.x - 1, y - 1, state->track.w + 2, 3};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, palette->selected_fill_r, palette->selected_fill_g,
                           palette->selected_fill_b, 255);
    SDL_RenderFillRect(renderer, &marker);
    SDL_SetRenderDrawColor(renderer, palette->selected_border_r, palette->selected_border_g,
                           palette->selected_border_b, 255);
    SDL_RenderDrawRect(renderer, &marker);
}
