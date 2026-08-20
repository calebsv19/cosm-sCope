#ifndef DATALAB_INPUT_CATALOG_H
#define DATALAB_INPUT_CATALOG_H

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>

#include "app/app_state.h"
#include "core_base.h"

/* Names live in a compact arena. Indices are valid only for the current
 * generation; callers copy names rather than retaining internal pointers. */
typedef enum DatalabInputCatalogRefreshReason {
    DATALAB_INPUT_CATALOG_REFRESH_NONE = 0,
    DATALAB_INPUT_CATALOG_REFRESH_INITIAL,
    DATALAB_INPUT_CATALOG_REFRESH_ROOT_CHANGE,
    DATALAB_INPUT_CATALOG_REFRESH_EXPLICIT,
    DATALAB_INPUT_CATALOG_REFRESH_FINGERPRINT_CHANGED
} DatalabInputCatalogRefreshReason;

typedef enum DatalabInputCatalogState {
    DATALAB_INPUT_CATALOG_IDLE = 0,
    DATALAB_INPUT_CATALOG_SCANNING,
    DATALAB_INPUT_CATALOG_MERGING,
    DATALAB_INPUT_CATALOG_READY,
    DATALAB_INPUT_CATALOG_CANCELED,
    DATALAB_INPUT_CATALOG_ERROR
} DatalabInputCatalogState;

typedef struct DatalabInputCatalogEntry {
    uint64_t name_offset;
    uint32_t name_length;
} DatalabInputCatalogEntry;

typedef struct DatalabInputCatalogRun {
    uint64_t *indexes;
    size_t count;
} DatalabInputCatalogRun;

typedef struct DatalabInputCatalogMetrics {
    size_t metadata_bytes;
    size_t name_bytes;
    size_t peak_bytes;
    size_t max_step_directory_entries;
    size_t max_step_merge_entries;
    size_t sorted_run_count;
} DatalabInputCatalogMetrics;

typedef struct DatalabInputCatalog {
    DatalabInputCatalogEntry *entries;
    uint64_t *order;
    char *names;
    char *root;
    size_t file_count;
    size_t entry_capacity;
    size_t name_bytes;
    size_t name_capacity;
    size_t sequence_gap_count;
    uint64_t directory_fingerprint;
    uint64_t last_scan_duration_us;
    uint64_t refresh_count;
    uint64_t generation;
    DatalabInputCatalogRefreshReason last_refresh_reason;
    DatalabInputCatalogState state;
    int fingerprint_valid;

    /* Private in-progress generation; published only after final merge. */
    DIR *scan_dir;
    DatalabInputCatalogEntry *scan_entries;
    char *scan_names;
    char *scan_root;
    uint64_t *scan_order;
    DatalabInputCatalogRun *scan_runs;
    size_t scan_count;
    size_t scan_entry_capacity;
    size_t scan_name_bytes;
    size_t scan_name_capacity;
    size_t scan_order_capacity;
    size_t scan_run_count;
    size_t scan_run_capacity;
    size_t scan_run_fill;
    size_t *merge_heap;
    size_t *merge_positions;
    size_t merge_heap_count;
    size_t merge_output_count;
    uint64_t scan_started_us;
    DatalabInputCatalogMetrics metrics;
} DatalabInputCatalog;

void datalab_input_catalog_init(DatalabInputCatalog *catalog);
void datalab_input_catalog_destroy(DatalabInputCatalog *catalog);
void datalab_input_catalog_clear(DatalabInputCatalog *catalog);

CoreResult datalab_input_catalog_begin_refresh(DatalabInputCatalog *catalog,
                                               const char *root,
                                               DatalabInputCatalogRefreshReason reason);
CoreResult datalab_input_catalog_step(DatalabInputCatalog *catalog, size_t work_limit);
void datalab_input_catalog_cancel(DatalabInputCatalog *catalog);
int datalab_input_catalog_is_busy(const DatalabInputCatalog *catalog);
int datalab_input_catalog_is_ready(const DatalabInputCatalog *catalog);
const char *datalab_input_catalog_state_name(DatalabInputCatalogState state);

/* Startup compatibility only; frame paths must use begin_refresh/step. */
CoreResult datalab_input_catalog_scan(DatalabInputCatalog *catalog, const char *root);
CoreResult datalab_input_catalog_refresh(DatalabInputCatalog *catalog,
                                         const char *root,
                                         DatalabInputCatalogRefreshReason reason);

size_t datalab_input_catalog_count(const DatalabInputCatalog *catalog);
int datalab_input_catalog_name_copy(const DatalabInputCatalog *catalog,
                                    uint64_t index,
                                    char *out_name,
                                    size_t out_name_cap);
size_t datalab_input_catalog_page_copy(const DatalabInputCatalog *catalog,
                                       uint64_t first_index,
                                       char (*out_names)[DATALAB_APP_PATH_CAP],
                                       size_t out_capacity);
int datalab_input_catalog_find_index(const DatalabInputCatalog *catalog,
                                     const char *name,
                                     uint64_t *out_index);
/* Legacy signed-index adapter; new callers use find_index. */
int datalab_input_catalog_find_name(const DatalabInputCatalog *catalog, const char *name);
int datalab_input_catalog_restore_selection(const DatalabInputCatalog *catalog,
                                            const char *previous_name,
                                            uint64_t previous_index,
                                            uint64_t *out_index);
const char *datalab_input_catalog_root(const DatalabInputCatalog *catalog);
const DatalabInputCatalogMetrics *datalab_input_catalog_metrics(const DatalabInputCatalog *catalog);

int datalab_input_catalog_root_matches(const DatalabInputCatalog *catalog, const char *root);
int datalab_input_catalog_fingerprint_changed(const DatalabInputCatalog *catalog, const char *root);
int datalab_input_catalog_file_is_current(const DatalabInputCatalog *catalog,
                                          const char *root,
                                          const char *name);
const char *datalab_input_catalog_refresh_reason_name(DatalabInputCatalogRefreshReason reason);

/* Deterministic storage/index scale proof; it never decodes pixels. */
CoreResult datalab_input_catalog_generate_fixture(DatalabInputCatalog *catalog, size_t count);
/* Contract-test fault injection; production callers never enable it. */
void datalab_input_catalog_test_fail_next_allocation(void);

#endif
