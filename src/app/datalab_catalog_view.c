#include "app/datalab_catalog_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/datalab_input_catalog.h"

static int datalab_catalog_view_match(const char *name, const char *filter) {
    size_t start = 0u;
    size_t filter_len = 0u;
    if (!filter || !filter[0]) return 1;
    if (!name) return 0;
    filter_len = strlen(filter);
    for (; name[start]; ++start) {
        size_t i = 0u;
        while (i < filter_len && name[start + i]) {
            unsigned char a = (unsigned char)name[start + i];
            unsigned char b = (unsigned char)filter[i];
            if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
            if (a != b) break;
            ++i;
        }
        if (i == filter_len) return 1;
    }
    return 0;
}

static uint64_t datalab_catalog_view_catalog_count(const DatalabCatalogView *view) {
    return view && view->catalog ? (uint64_t)datalab_input_catalog_count(view->catalog) : 0u;
}

static void datalab_catalog_view_reset_page(DatalabCatalogView *view) {
    if (!view) return;
    view->page_first = 0u;
    view->page_count = 0u;
}

static int datalab_catalog_view_grow_matches(DatalabCatalogView *view, size_t need) {
    size_t cap = 0u;
    uint64_t *replacement = NULL;
    if (!view) return 0;
    if (need <= view->match_capacity) return 1;
    cap = view->match_capacity ? view->match_capacity : 256u;
    while (cap < need) {
        if (cap > SIZE_MAX / 2u) return 0;
        cap *= 2u;
    }
    replacement = realloc(view->match_indices, cap * sizeof(*replacement));
    if (!replacement) return 0;
    view->match_indices = replacement;
    view->match_capacity = cap;
    return 1;
}

static uint64_t datalab_catalog_view_resolve(const DatalabCatalogView *view, uint64_t visible_index) {
    if (!view || visible_index >= datalab_catalog_view_count(view)) return UINT64_MAX;
    return view->filter[0] ? view->match_indices[visible_index] : visible_index;
}

static void datalab_catalog_view_restore_selection(DatalabCatalogView *view) {
    uint64_t restored = 0u;
    uint64_t count = 0u;
    if (!view) return;
    count = datalab_catalog_view_count(view);
    if (count == 0u) { view->selected_index = 0u; view->selected_name[0] = '\0'; return; }
    if (view->selected_name[0] && view->catalog &&
        datalab_input_catalog_find_index(view->catalog, view->selected_name, &restored)) {
        if (!view->filter[0]) view->selected_index = restored;
        else {
            for (size_t i = 0u; i < view->match_count; ++i) {
                if (view->match_indices[i] == restored) { view->selected_index = (uint64_t)i; return; }
            }
        }
    }
    if (view->selected_index >= count) view->selected_index = count - 1u;
}

void datalab_catalog_view_init(DatalabCatalogView *view) { if (view) memset(view, 0, sizeof(*view)); }
void datalab_catalog_view_destroy(DatalabCatalogView *view) {
    if (!view) return;
    free(view->match_indices);
    datalab_catalog_view_init(view);
}

void datalab_catalog_view_bind(DatalabCatalogView *view, const DatalabInputCatalog *catalog) {
    if (!view) return;
    if (view->catalog == catalog && (!catalog || view->catalog_generation == catalog->generation)) return;
    view->catalog = catalog;
    view->catalog_generation = catalog ? catalog->generation : 0u;
    view->match_count = 0u;
    view->filter_cursor = 0u;
    view->filter_scanning = view->filter[0] && datalab_catalog_view_catalog_count(view) > 0u;
    view->metrics.filter_scanned = 0u;
    view->metrics.filter_total = datalab_catalog_view_catalog_count(view);
    view->metrics.filter_matches = 0u;
    view->metrics.generation = view->catalog_generation;
    datalab_catalog_view_reset_page(view);
    if (!view->filter_scanning) datalab_catalog_view_restore_selection(view);
}

void datalab_catalog_view_set_filter(DatalabCatalogView *view, const char *filter) {
    if (!view) return;
    snprintf(view->filter, sizeof(view->filter), "%s", filter ? filter : "");
    view->match_count = 0u;
    view->filter_cursor = 0u;
    view->filter_scanning = view->filter[0] && datalab_catalog_view_catalog_count(view) > 0u;
    view->metrics.filter_scanned = 0u;
    view->metrics.filter_total = datalab_catalog_view_catalog_count(view);
    view->metrics.filter_matches = 0u;
    datalab_catalog_view_reset_page(view);
    if (!view->filter_scanning) datalab_catalog_view_restore_selection(view);
}

void datalab_catalog_view_step_filter(DatalabCatalogView *view, size_t work_limit) {
    char name[DATALAB_APP_PATH_CAP];
    uint64_t total = 0u;
    size_t done = 0u;
    if (!view || !view->filter_scanning || !view->catalog || work_limit == 0u) return;
    if (view->catalog_generation != view->catalog->generation) { datalab_catalog_view_bind(view, view->catalog); return; }
    total = datalab_catalog_view_catalog_count(view);
    while (view->filter_cursor < total && done++ < work_limit) {
        uint64_t raw = view->filter_cursor++;
        if (datalab_input_catalog_name_copy(view->catalog, raw, name, sizeof(name)) &&
            datalab_catalog_view_match(name, view->filter)) {
            if (!datalab_catalog_view_grow_matches(view, view->match_count + 1u)) { view->filter_scanning = 0; break; }
            view->match_indices[view->match_count++] = raw;
        }
    }
    view->metrics.filter_scanned = view->filter_cursor;
    view->metrics.filter_matches = view->match_count;
    if (view->filter_cursor >= total) { view->filter_scanning = 0; datalab_catalog_view_restore_selection(view); }
}

int datalab_catalog_view_filter_scanning(const DatalabCatalogView *view) { return view ? view->filter_scanning : 0; }
uint64_t datalab_catalog_view_count(const DatalabCatalogView *view) {
    if (!view) return 0u;
    return view->filter[0] ? (uint64_t)view->match_count : datalab_catalog_view_catalog_count(view);
}
uint64_t datalab_catalog_view_selected_index(const DatalabCatalogView *view) { return view ? view->selected_index : 0u; }
void datalab_catalog_view_set_selected_index(DatalabCatalogView *view, uint64_t index) {
    uint64_t count = datalab_catalog_view_count(view);
    if (!view) return;
    view->selected_index = count ? (index < count ? index : count - 1u) : 0u;
    (void)datalab_catalog_view_selected_name_copy(view, view->selected_name, sizeof(view->selected_name));
}
void datalab_catalog_view_step_selected(DatalabCatalogView *view, int64_t delta, int wrap) {
    uint64_t count = datalab_catalog_view_count(view);
    int64_t next = 0;
    if (!view || count == 0u) return;
    next = (int64_t)view->selected_index + delta;
    if (wrap) { next %= (int64_t)count; if (next < 0) next += (int64_t)count; }
    else { if (next < 0) next = 0; if ((uint64_t)next >= count) next = (int64_t)count - 1; }
    datalab_catalog_view_set_selected_index(view, (uint64_t)next);
}
int datalab_catalog_view_selected_name_copy(DatalabCatalogView *view, char *out, size_t cap) {
    return datalab_catalog_view_name_copy(view, datalab_catalog_view_selected_index(view), out, cap);
}
int datalab_catalog_view_name_copy(DatalabCatalogView *view, uint64_t visible_index, char *out, size_t cap) {
    uint64_t raw = datalab_catalog_view_resolve(view, visible_index);
    if (!out || cap == 0u) return 0;
    out[0] = '\0';
    return raw != UINT64_MAX && view && view->catalog && datalab_input_catalog_name_copy(view->catalog, raw, out, cap);
}
size_t datalab_catalog_view_page_copy(DatalabCatalogView *view, uint64_t first, size_t rows, char (*out)[DATALAB_APP_PATH_CAP], size_t cap) {
    size_t want = 0u;
    if (!view || !out || cap == 0u) return 0u;
    want = rows < cap ? rows : cap;
    if (want > DATALAB_CATALOG_VIEW_PAGE_WINDOW) want = DATALAB_CATALOG_VIEW_PAGE_WINDOW;
    for (size_t i = 0u; i < want && first + i < datalab_catalog_view_count(view); ++i) {
        if (!datalab_catalog_view_name_copy(view, first + i, out[i], DATALAB_APP_PATH_CAP)) return i;
        if (out != view->page) snprintf(view->page[i], sizeof(view->page[i]), "%s", out[i]);
    }
    view->page_first = first;
    view->page_count = want;
    view->metrics.page_fetches++;
    view->metrics.visible_rows = want;
    return want;
}
const DatalabCatalogViewMetrics *datalab_catalog_view_metrics(const DatalabCatalogView *view) { return view ? &view->metrics : NULL; }
