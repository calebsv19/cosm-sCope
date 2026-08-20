#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/datalab_input_catalog.h"
#include "app/datalab_catalog_view.h"

static int expect(int condition, const char *message) {
    if (!condition) fprintf(stderr, "datalab input catalog contract failed: %s\n", message);
    return condition;
}

static int write_fixture(const char *root, size_t count) {
    char path[DATALAB_APP_PATH_CAP];
    for (size_t i = 0u; i < count; ++i) {
        FILE *file = NULL;
        snprintf(path, sizeof(path), "%s/frame_%04zu.%s", root, count - i, (i % 3u) == 0u ? "png" : ((i % 3u) == 1u ? "bmp" : "pack"));
        file = fopen(path, "wb");
        if (!file) return 0;
        fputs("fixture", file);
        fclose(file);
    }
    return 1;
}

static int test_incremental_directory_contract(void) {
    char root[] = "/private/tmp/datalab-catalog.XXXXXX";
    DatalabInputCatalog catalog;
    char page[4][DATALAB_APP_PATH_CAP] = {{0}};
    char name[DATALAB_APP_PATH_CAP] = {0};
    CoreResult result;
    uint64_t restored = 0u;
    int ok = 1;
    if (!mkdtemp(root) || !write_fixture(root, 257u)) return 0;
    datalab_input_catalog_init(&catalog);
    result = datalab_input_catalog_begin_refresh(&catalog, root, DATALAB_INPUT_CATALOG_REFRESH_INITIAL);
    ok &= expect(result.code == CORE_OK && datalab_input_catalog_is_busy(&catalog), "scan must start asynchronously");
    result = datalab_input_catalog_step(&catalog, 64u);
    ok &= expect(result.code == CORE_OK && catalog.metrics.max_step_directory_entries <= 64u, "directory step must remain bounded");
    datalab_input_catalog_cancel(&catalog);
    ok &= expect(catalog.state == DATALAB_INPUT_CATALOG_CANCELED && catalog.file_count == 0u, "cancel must discard unpublished generation");
    result = datalab_input_catalog_begin_refresh(&catalog, root, DATALAB_INPUT_CATALOG_REFRESH_EXPLICIT);
    while (result.code == CORE_OK && datalab_input_catalog_is_busy(&catalog)) result = datalab_input_catalog_step(&catalog, 65u);
    ok &= expect(result.code == CORE_OK && datalab_input_catalog_count(&catalog) == 257u, "restart must publish the full logical catalog beyond 256");
    ok &= expect(datalab_input_catalog_page_copy(&catalog, 63u, page, 4u) == 4u && strcmp(page[0], "frame_0064.bmp") == 0, "indexed pages must use deterministic natural order");
    ok &= expect(datalab_input_catalog_name_copy(&catalog, 256u, name, sizeof(name)) && strcmp(name, "frame_0257.png") == 0, "last logical index must remain available");
    ok &= expect(datalab_input_catalog_restore_selection(&catalog, "frame_0128.png", 0u, &restored) && restored == 127u, "selection must restore by identity");
    ok &= expect(catalog.metrics.max_step_merge_entries <= 65u && catalog.metrics.metadata_bytes / catalog.file_count < 64u, "merge work and metadata stay bounded and compact");
    datalab_input_catalog_destroy(&catalog);
    for (size_t i = 0u; i < 257u; ++i) { char path[DATALAB_APP_PATH_CAP]; snprintf(path, sizeof(path), "%s/frame_%04zu.%s", root, 257u-i, (i%3u)==0u?"png":((i%3u)==1u?"bmp":"pack")); unlink(path); }
    rmdir(root);
    return ok;
}

static int test_allocation_failure_contract(void) {
    char root[] = "/private/tmp/datalab-catalog-oom.XXXXXX";
    char path[DATALAB_APP_PATH_CAP];
    DatalabInputCatalog catalog;
    CoreResult result;
    FILE *file = NULL;
    int ok = 1;
    if (!mkdtemp(root)) return 0;
    snprintf(path, sizeof(path), "%s/frame_0001.bmp", root);
    file = fopen(path, "wb");
    if (!file || fclose(file) != 0) return 0;
    datalab_input_catalog_init(&catalog);
    result = datalab_input_catalog_begin_refresh(&catalog, root, DATALAB_INPUT_CATALOG_REFRESH_INITIAL);
    datalab_input_catalog_test_fail_next_allocation();
    result = result.code == CORE_OK ? datalab_input_catalog_step(&catalog, 8u) : result;
    ok &= expect(result.code == CORE_ERR_OUT_OF_MEMORY && catalog.state == DATALAB_INPUT_CATALOG_ERROR,
                 "allocation failure must be explicit and fail closed");
    datalab_input_catalog_destroy(&catalog);
    unlink(path); rmdir(root);
    return ok;
}

static int test_fixture_scale(size_t count) {
    DatalabInputCatalog catalog;
    CoreResult result;
    char first[DATALAB_APP_PATH_CAP] = {0}, last[DATALAB_APP_PATH_CAP] = {0};
    int ok = 1;
    datalab_input_catalog_init(&catalog);
    result = datalab_input_catalog_generate_fixture(&catalog, count);
    ok &= expect(result.code == CORE_OK && catalog.file_count == count, "synthetic catalog must retain every logical entry");
    ok &= expect(datalab_input_catalog_name_copy(&catalog, 0u, first, sizeof(first)) && datalab_input_catalog_name_copy(&catalog, count - 1u, last, sizeof(last)), "synthetic first and last lookups must succeed");
    if (strcmp(first, "frame_0000000001.png") != 0 || strstr(last, "frame_") != last) {
        fprintf(stderr, "catalog-scale ordering count=%zu first=%s last=%s\n", count, first, last);
        ok &= expect(0, "synthetic natural order must be stable");
    }
    ok &= expect(catalog.metrics.metadata_bytes + catalog.metrics.name_bytes < count * 128u, "compact catalog storage must stay well below 1024 bytes per entry");
    printf("catalog-scale count=%zu metadata=%zu names=%zu peak=%zu bytes_per_entry=%.2f steps(dir=%zu merge=%zu) runs=%zu\n", count, catalog.metrics.metadata_bytes, catalog.metrics.name_bytes, catalog.metrics.peak_bytes, (double)(catalog.metrics.metadata_bytes + catalog.metrics.name_bytes) / (double)count, catalog.metrics.max_step_directory_entries, catalog.metrics.max_step_merge_entries, catalog.metrics.sorted_run_count);
    datalab_input_catalog_destroy(&catalog);
    return ok;
}

static int test_virtual_view_contract(void) {
    DatalabInputCatalog catalog;
    DatalabCatalogView view;
    char page[DATALAB_CATALOG_VIEW_PAGE_WINDOW][DATALAB_APP_PATH_CAP] = {{0}};
    char selected[DATALAB_APP_PATH_CAP] = {0};
    int ok = 1;
    datalab_input_catalog_init(&catalog);
    datalab_catalog_view_init(&view);
    ok &= expect(datalab_input_catalog_generate_fixture(&catalog, 1000000u).code == CORE_OK,
                 "million-entry virtual-view fixture must build");
    datalab_catalog_view_bind(&view, &catalog);
    datalab_catalog_view_set_selected_index(&view, 256u);
    ok &= expect(datalab_catalog_view_selected_name_copy(&view, selected, sizeof(selected)) &&
                     strcmp(selected, "frame_0000000257.png") == 0,
                 "logical selection must cross the 256/257 boundary");
    ok &= expect(datalab_catalog_view_page_copy(&view, 255u, 3u, page, DATALAB_CATALOG_VIEW_PAGE_WINDOW) == 3u &&
                     strcmp(page[0], "frame_0000000256.png") == 0 &&
                     strcmp(page[2], "frame_0000000258.png") == 0,
                 "visible page must fetch only the requested boundary rows");
    datalab_catalog_view_step_selected(&view, -256, 0);
    ok &= expect(datalab_catalog_view_selected_index(&view) == 0u, "Home-equivalent clamp must reach first item");
    datalab_catalog_view_step_selected(&view, -1, 1);
    ok &= expect(datalab_catalog_view_selected_index(&view) == 999999u, "cycle must wrap first to last");
    datalab_catalog_view_step_selected(&view, 1, 1);
    ok &= expect(datalab_catalog_view_selected_index(&view) == 0u, "cycle must wrap last to first");
    datalab_catalog_view_set_filter(&view, "frame_0001000000");
    while (datalab_catalog_view_filter_scanning(&view)) datalab_catalog_view_step_filter(&view, 4096u);
    ok &= expect(datalab_catalog_view_count(&view) == 1u &&
                     datalab_catalog_view_name_copy(&view, 0u, selected, sizeof(selected)) &&
                     strcmp(selected, "frame_0001000000.png") == 0,
                 "incremental filter must find a match at the million-entry end");
    datalab_catalog_view_set_filter(&view, "frame_0000000257");
    while (datalab_catalog_view_filter_scanning(&view)) datalab_catalog_view_step_filter(&view, 4096u);
    ok &= expect(datalab_catalog_view_count(&view) == 1u,
                 "filter must find matches beyond the old 256-item cap");
    datalab_catalog_view_set_filter(&view, "");
    datalab_catalog_view_set_selected_index(&view, 256u);
    ok &= expect(datalab_input_catalog_generate_fixture(&catalog, 256u).code == CORE_OK,
                 "removal fixture must rebuild");
    datalab_catalog_view_bind(&view, &catalog);
    ok &= expect(datalab_catalog_view_selected_index(&view) == 255u,
                 "removed selection must deterministically fall back to the last index");
    ok &= expect(sizeof(view.page) == DATALAB_CATALOG_VIEW_PAGE_WINDOW * DATALAB_APP_PATH_CAP &&
                     datalab_catalog_view_metrics(&view)->page_fetches > 0u,
                 "render storage must remain one bounded page with explicit fetch metrics");
    datalab_catalog_view_destroy(&view);
    datalab_input_catalog_destroy(&catalog);
    return ok;
}

int main(void) {
    int ok = test_incremental_directory_contract();
    ok &= test_allocation_failure_contract();
    DatalabInputCatalog empty;
    datalab_input_catalog_init(&empty);
    ok &= expect(datalab_input_catalog_generate_fixture(&empty, 0u).code == CORE_OK &&
                     datalab_input_catalog_count(&empty) == 0u,
                 "empty catalog must be representable");
    datalab_input_catalog_destroy(&empty);
    const size_t boundaries[] = {1u, 64u, 65u, 160u, 161u, 256u, 257u, 10000u, 100000u, 1000000u};
    for (size_t i = 0u; ok && i < sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
        ok &= test_fixture_scale(boundaries[i]);
    }
    ok &= test_virtual_view_contract();
    return ok ? 0 : 1;
}
