#ifndef DATALAB_PICKER_INTERACTION_H
#define DATALAB_PICKER_INTERACTION_H
#include "render/render_view_ui_surface.h"
#include "app/datalab_catalog_view.h"
#include "render/render_view_picker_support.h"
enum { DATALAB_PICKER_ROOT=1, DATALAB_PICKER_FILE=2 };
typedef struct DatalabPickerRecentUiState {
    SDL_Rect list_rect, item_rects[DATALAB_RECENT_INPUT_ROOT_LIMIT];
    size_t visible_count, first_index;
} DatalabPickerRecentUiState;
typedef struct DatalabPickerRecentFileUiState {
    SDL_Rect list_rect, item_rects[4];
    size_t visible_count;
} DatalabPickerRecentFileUiState;
typedef struct DatalabPickerActionHost {
    DatalabInputCatalog *catalog; DatalabCatalogView *view;
    char *input_root, *edit_root, *status, *output;
    size_t root_cap, status_cap, output_cap;
    char (*roots)[DATALAB_APP_PATH_CAP]; size_t *root_count;
    char (*files)[DATALAB_APP_PATH_CAP]; size_t file_count;
    const char *filter;
    uint64_t *all_count, *filtered_count, *selected;
    int *reveal, *edit_mode, *done;
} DatalabPickerActionHost;
void datalab_picker_collect_buttons(KitUiSurface *surface, int width, int height,
    const DatalabPickerRecentUiState *recent, char roots[][DATALAB_APP_PATH_CAP], size_t root_count,
    const DatalabPickerRecentFileUiState *files_ui, char files[][DATALAB_APP_PATH_CAP], size_t file_count);
int datalab_picker_route_buttons(KitUiSurface *surface, SDL_Window *window, SDL_Renderer *renderer,
    const SDL_Event *event, int editing, KitUiSurfaceKey *out_action);
void datalab_picker_draw_row(KitUiSurface *surface, SDL_Renderer *renderer, SDL_Rect rect,
    uint32_t domain, const char *path, int selected, const DatalabPickerThemePalette *palette);
void datalab_picker_apply_button(DatalabPickerActionHost *host, KitUiSurfaceKey key);
int datalab_picker_refresh_catalog(DatalabInputCatalog *catalog, DatalabCatalogView *view,
    const char *root, DatalabInputCatalogRefreshReason reason, char *status, size_t status_cap);
void datalab_picker_view_apply_filter(DatalabCatalogView *view, const char *filter, uint64_t *out_count);
#endif
