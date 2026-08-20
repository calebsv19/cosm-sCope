#ifndef DATALAB_CATALOG_VIEW_H
#define DATALAB_CATALOG_VIEW_H

#include <stddef.h>
#include <stdint.h>

#include "app/app_state.h"

struct DatalabInputCatalog;

/* UI ownership is deliberately bounded.  This is a caller-visible page, not
 * a catalog-size limit. */
enum { DATALAB_CATALOG_VIEW_PAGE_WINDOW = 96, DATALAB_CATALOG_VIEW_FILTER_TEXT_CAP = 128 };

typedef struct DatalabCatalogViewMetrics {
    uint64_t visible_rows;
    uint64_t page_fetches;
    uint64_t filter_scanned;
    uint64_t filter_total;
    uint64_t filter_matches;
    uint64_t generation;
} DatalabCatalogViewMetrics;

typedef struct DatalabCatalogView {
    const struct DatalabInputCatalog *catalog;
    uint64_t catalog_generation;
    uint64_t *match_indices;
    size_t match_count;
    size_t match_capacity;
    uint64_t filter_cursor;
    uint64_t selected_index;
    char selected_name[DATALAB_APP_PATH_CAP];
    char filter[DATALAB_CATALOG_VIEW_FILTER_TEXT_CAP];
    char page[DATALAB_CATALOG_VIEW_PAGE_WINDOW][DATALAB_APP_PATH_CAP];
    uint64_t page_first;
    size_t page_count;
    int filter_scanning;
    DatalabCatalogViewMetrics metrics;
} DatalabCatalogView;

void datalab_catalog_view_init(DatalabCatalogView *view);
void datalab_catalog_view_destroy(DatalabCatalogView *view);
void datalab_catalog_view_bind(DatalabCatalogView *view,
                               const struct DatalabInputCatalog *catalog);
void datalab_catalog_view_set_filter(DatalabCatalogView *view, const char *filter);
/* Advances a filter incrementally.  A new filter/bind cancels prior work. */
void datalab_catalog_view_step_filter(DatalabCatalogView *view, size_t work_limit);
int datalab_catalog_view_filter_scanning(const DatalabCatalogView *view);
uint64_t datalab_catalog_view_count(const DatalabCatalogView *view);
uint64_t datalab_catalog_view_selected_index(const DatalabCatalogView *view);
void datalab_catalog_view_set_selected_index(DatalabCatalogView *view, uint64_t index);
void datalab_catalog_view_step_selected(DatalabCatalogView *view, int64_t delta, int wrap);
int datalab_catalog_view_selected_name_copy(DatalabCatalogView *view,
                                            char *out_name,
                                            size_t out_name_cap);
int datalab_catalog_view_name_copy(DatalabCatalogView *view,
                                   uint64_t visible_index,
                                   char *out_name,
                                   size_t out_name_cap);
/* Fetches exactly the requested visible window (clamped to one page). */
size_t datalab_catalog_view_page_copy(DatalabCatalogView *view,
                                      uint64_t first_visible_index,
                                      size_t requested_rows,
                                      char (*out_names)[DATALAB_APP_PATH_CAP],
                                      size_t out_capacity);
const DatalabCatalogViewMetrics *datalab_catalog_view_metrics(const DatalabCatalogView *view);

#endif
