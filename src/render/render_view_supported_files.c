#include "render/render_view_internal.h"

#include <stdio.h>
#include <string.h>

#include "app/datalab_input_catalog.h"

DatalabSupportedFileScanResult datalab_scan_supported_files(const char *root,
                                                            char files[][DATALAB_APP_PATH_CAP],
                                                            size_t max_files) {
    DatalabSupportedFileScanResult result = {0};
    DatalabInputCatalog catalog;
    CoreResult scan_result;
    size_t copy_count = 0u;
    if (!root || !files || max_files == 0u) {
        result.invalid_request = 1;
        return result;
    }
    datalab_input_catalog_init(&catalog);
    scan_result = datalab_input_catalog_scan(&catalog, root);
    if (scan_result.code != CORE_OK) {
        result.root_unavailable = scan_result.code == CORE_ERR_IO;
        result.allocation_failed = scan_result.code == CORE_ERR_OUT_OF_MEMORY;
        result.invalid_request = scan_result.code == CORE_ERR_INVALID_ARG;
        datalab_input_catalog_destroy(&catalog);
        return result;
    }
    result.available_file_count = datalab_input_catalog_count(&catalog);
    result.sequence_gap_count = catalog.sequence_gap_count;
    copy_count = datalab_input_catalog_page_copy(&catalog, 0u, files, max_files);
    result.file_count = copy_count;
    result.truncated = copy_count < result.available_file_count;
    datalab_input_catalog_destroy(&catalog);
    return result;
}

void datalab_format_supported_file_count_status(size_t file_count, char *status, size_t status_cap) {
    if (!status || status_cap == 0u) {
        return;
    }
    snprintf(status, status_cap, "found %zu supported files (.pack/.bmp/.png)", file_count);
}

void datalab_format_supported_file_scan_status(const DatalabSupportedFileScanResult *scan,
                                               const char *root,
                                               const char *recovery_hint,
                                               char *status,
                                               size_t status_cap) {
    const char *display_root = (root && root[0] != '\0') ? root : "(empty)";
    if (!status || status_cap == 0u) {
        return;
    }
    if (!scan) {
        snprintf(status, status_cap, "invalid file scan request");
        return;
    }
    if (scan->invalid_request) {
        snprintf(status, status_cap, "invalid file scan request (root=%s)", display_root);
        return;
    }
    if (scan->root_unavailable) {
        if (recovery_hint && recovery_hint[0] != '\0') {
            snprintf(status,
                     status_cap,
                     "input root unavailable: %s (%s)",
                     display_root,
                     recovery_hint);
        } else {
            snprintf(status, status_cap, "input root unavailable: %s", display_root);
        }
        return;
    }
    if (scan->allocation_failed) {
        snprintf(status, status_cap, "input catalog unavailable: out of memory");
        return;
    }
    if (scan->truncated) {
        snprintf(status,
                 status_cap,
                 "showing %zu of %zu supported files (.pack/.bmp/.png)",
                 scan->file_count,
                 scan->available_file_count);
        return;
    }
    if (scan->sequence_gap_count > 0u) {
        snprintf(status, status_cap, "found %zu supported files; detected %zu numbered-frame gap%s",
                 scan->file_count, scan->sequence_gap_count, scan->sequence_gap_count == 1u ? "" : "s");
        return;
    }
    datalab_format_supported_file_count_status(scan->file_count, status, status_cap);
}
