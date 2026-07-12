#ifndef DATALAB_RENDER_VIEW_PICKER_PANES_H
#define DATALAB_RENDER_VIEW_PICKER_PANES_H

#include <SDL2/SDL.h>

#include "render_view_picker_support.h"

typedef struct DatalabPickerScrollState {
    SDL_Rect viewport;
    SDL_Rect track;
    SDL_Rect thumb;
    int offset_rows;
    int content_rows;
    int visible_rows;
    int drag_active;
    int drag_origin_y;
    int drag_origin_offset;
} DatalabPickerScrollState;

void datalab_picker_scroll_configure(DatalabPickerScrollState *state,
                                     SDL_Rect viewport,
                                     int content_rows,
                                     int visible_rows,
                                     int row_height);
void datalab_picker_scroll_by(DatalabPickerScrollState *state, int delta_rows);
int datalab_picker_scroll_begin_drag(DatalabPickerScrollState *state, int x, int y);
void datalab_picker_scroll_drag_to(DatalabPickerScrollState *state, int y);
void datalab_picker_scroll_end_drag(DatalabPickerScrollState *state);
void datalab_picker_scroll_draw(SDL_Renderer *renderer,
                                const DatalabPickerScrollState *state,
                                const DatalabPickerThemePalette *palette);
void datalab_picker_scroll_draw_selection_marker(SDL_Renderer *renderer,
                                                 const DatalabPickerScrollState *state,
                                                 int selected_row,
                                                 const DatalabPickerThemePalette *palette);

#endif
