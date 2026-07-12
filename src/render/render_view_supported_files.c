#include "render/render_view_internal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "data/input_file_loader.h"

static int datalab_supported_file_name_ci_cmp(const void *a, const void *b) {
    const char *aa = (const char *)a;
    const char *bb = (const char *)b;
    return strcasecmp(aa, bb);
}

DatalabSupportedFileScanResult datalab_scan_supported_files(const char *root,
                                                            char files[][DATALAB_APP_PATH_CAP],
                                                            size_t max_files) {
    DatalabSupportedFileScanResult result = {0u, 0, 0};
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!root || !files || max_files == 0u) {
        result.invalid_request = 1;
        return result;
    }
    dir = opendir(root);
    if (!dir) {
        result.root_unavailable = 1;
        return result;
    }
    while ((entry = readdir(dir)) != NULL) {
        char child_path[DATALAB_APP_PATH_CAP];
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!datalab_input_root_join_child_file(root,
                                               entry->d_name,
                                               child_path,
                                               sizeof(child_path))) {
            continue;
        }
        if (!datalab_input_file_is_supported(entry->d_name)) {
            continue;
        }
        if (result.file_count >= max_files) {
            break;
        }
        snprintf(files[result.file_count], DATALAB_APP_PATH_CAP, "%s", entry->d_name);
        result.file_count++;
    }
    closedir(dir);
    if (result.file_count > 1u) {
        qsort(files, result.file_count, sizeof(files[0]), datalab_supported_file_name_ci_cmp);
    }
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
    datalab_format_supported_file_count_status(scan->file_count, status, status_cap);
}
