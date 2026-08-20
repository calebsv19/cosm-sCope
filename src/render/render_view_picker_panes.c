#include "render_view_picker_panes.h"

#include <limits.h>
#include "kit_ui_sdl.h"

static uint64_t datalab_picker_scroll_max_offset(const DatalabPickerScrollState *state) {
    return state && state->content_rows > state->visible_rows ? state->content_rows - state->visible_rows : 0u;
}

void datalab_picker_scroll_configure(DatalabPickerScrollState *state,
                                     SDL_Rect viewport,
                                     uint64_t content_rows,
                                     uint64_t visible_rows,
                                     int row_height) {
    uint64_t max_offset = 0u;
    KitUiSdlScrollbarLayout layout = {0};
    if (!state) return;
    state->viewport = viewport;
    state->content_rows = content_rows;
    state->visible_rows = visible_rows > 0u ? visible_rows : 1u;
    max_offset = datalab_picker_scroll_max_offset(state);
    if (state->offset_rows > max_offset) state->offset_rows = max_offset;
    if (row_height < 1) row_height = 1;
    kit_ui_sdl_scrollbar_layout(&viewport,
                                state->content_rows > (uint64_t)INT_MAX / (uint64_t)row_height ? INT_MAX : (int)(state->content_rows * (uint64_t)row_height),
                                state->offset_rows > (uint64_t)INT_MAX / (uint64_t)row_height ? INT_MAX : (int)(state->offset_rows * (uint64_t)row_height),
                                &layout);
    state->track = layout.track;
    state->thumb = layout.thumb;
}

void datalab_picker_scroll_by(DatalabPickerScrollState *state, int64_t delta_rows) {
    uint64_t max_offset = 0u;
    int64_t next = 0;
    if (!state) return;
    max_offset = datalab_picker_scroll_max_offset(state);
    next = (int64_t)state->offset_rows + delta_rows;
    if (next < 0) next = 0;
    state->offset_rows = (uint64_t)next > max_offset ? max_offset : (uint64_t)next;
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
    uint64_t max_offset = 0u;
    int travel = 0;
    if (!state || !state->drag_active) return;
    max_offset = datalab_picker_scroll_max_offset(state);
    travel = state->track.h - state->thumb.h;
    if (max_offset == 0u || travel <= 0) return;
    if (y <= state->drag_origin_y) {
        const uint64_t decrease = ((uint64_t)(state->drag_origin_y - y) * max_offset) / (uint64_t)travel;
        state->offset_rows = decrease >= state->drag_origin_offset ? 0u : state->drag_origin_offset - decrease;
    }
    else state->offset_rows = state->drag_origin_offset +
        ((uint64_t)(y - state->drag_origin_y) * max_offset) / (uint64_t)travel;
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
                                                 uint64_t selected_row,
                                                 const DatalabPickerThemePalette *palette) {
    SDL_Rect marker;
    int y = 0;
    if (!renderer || !state || !palette || state->content_rows <= state->visible_rows ||
        selected_row >= state->content_rows || state->track.h <= 0) return;
    y = state->track.y + (int)((selected_row * (uint64_t)(state->track.h - 1)) / (state->content_rows - 1u));
    marker = (SDL_Rect){state->track.x - 1, y - 1, state->track.w + 2, 3};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, palette->selected_fill_r, palette->selected_fill_g,
                           palette->selected_fill_b, 255);
    SDL_RenderFillRect(renderer, &marker);
    SDL_SetRenderDrawColor(renderer, palette->selected_border_r, palette->selected_border_g,
                           palette->selected_border_b, 255);
    SDL_RenderDrawRect(renderer, &marker);
}
