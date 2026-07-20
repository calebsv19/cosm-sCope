#include "app/datalab_runtime_prefs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>

#include "datalab/datalab_app_main.h"
#include "render/render_view.h"

static const char *k_datalab_text_zoom_step_path = "data/runtime/text_zoom_step.txt";
static const char *k_datalab_input_root_path = "data/runtime/input_root.txt";
static const char *k_datalab_recent_input_roots_path = "data/runtime/recent_input_roots_v1.txt";
static const char *k_datalab_recent_input_files_path = "data/runtime/recent_input_files_v1.txt";
static const char *k_datalab_pinned_input_files_path = "data/runtime/pinned_input_files_v1.txt";
static const char *k_datalab_theme_preset_path = "data/runtime/theme_preset_id.txt";
static const char *k_datalab_custom_theme_path = "data/runtime/custom_theme_v1.txt";
static const char *k_datalab_custom_theme_slots_path = "data/runtime/custom_theme_slots_v1.txt";
static const char *k_datalab_custom_theme_slot_names_path = "data/runtime/custom_theme_slot_names_v1.txt";
static const char *k_datalab_custom_theme_active_slot_path = "data/runtime/custom_theme_active_slot.txt";
static char g_datalab_runtime_prefs_diagnostic[192];

static int datalab_ensure_runtime_dirs(void);

static const char *datalab_runtime_prefs_path_basename(const char *path) {
    const char *last = NULL;
    if (!path) {
        return "(null)";
    }
    last = strrchr(path, '/');
    return last ? last + 1 : path;
}

static void datalab_runtime_prefs_set_diagnostic(const char *operation,
                                                 const char *path,
                                                 const char *detail) {
    (void)snprintf(g_datalab_runtime_prefs_diagnostic,
                   sizeof(g_datalab_runtime_prefs_diagnostic),
                   "prefs %s failed: %s (path=%s)",
                   operation ? operation : "operation",
                   detail ? detail : "unknown",
                   datalab_runtime_prefs_path_basename(path));
    fprintf(stderr, "datalab: %s\n", g_datalab_runtime_prefs_diagnostic);
}

static int datalab_runtime_prefs_open_for_load(const char *path, FILE **out_fp) {
    FILE *fp = NULL;
    int open_errno = 0;
    if (!path || !out_fp) {
        return 0;
    }
    *out_fp = NULL;
    fp = fopen(path, "rb");
    if (!fp) {
        open_errno = errno;
        if (open_errno != ENOENT) {
            datalab_runtime_prefs_set_diagnostic("load", path, strerror(open_errno));
        }
        return 0;
    }
    *out_fp = fp;
    return 1;
}

static FILE *datalab_runtime_prefs_open_for_save(const char *path) {
    FILE *fp = NULL;
    int open_errno = 0;
    if (!datalab_ensure_runtime_dirs()) {
        datalab_runtime_prefs_set_diagnostic("save", path, strerror(errno));
        return NULL;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        open_errno = errno;
        datalab_runtime_prefs_set_diagnostic("save", path, strerror(open_errno));
        return NULL;
    }
    return fp;
}

static int datalab_runtime_prefs_close_saved(FILE *fp, const char *path) {
    if (!fp) {
        return 0;
    }
    if (fclose(fp) != 0) {
        datalab_runtime_prefs_set_diagnostic("save", path, strerror(errno));
        return 0;
    }
    return 1;
}

static int datalab_runtime_prefs_write_ok(int write_rc, FILE *fp, const char *path) {
    if (write_rc < 0 || ferror(fp)) {
        datalab_runtime_prefs_set_diagnostic("save", path, "write failed");
        return 0;
    }
    return 1;
}

static uint8_t datalab_theme_preset_clamp(int value) {
    if (value < (int)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT) {
        return (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_DAW_DEFAULT;
    }
    if (value > (int)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM) {
        return (uint8_t)DATALAB_WORKSPACE_AUTHORING_THEME_CUSTOM;
    }
    return (uint8_t)value;
}

static uint8_t datalab_custom_theme_slot_clamp(int value) {
    if (value < 0) {
        return 0u;
    }
    if (value >= DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return (uint8_t)(DATALAB_CUSTOM_THEME_SLOT_COUNT - 1);
    }
    return (uint8_t)value;
}

static int datalab_input_root_path_equals(const char *lhs, const char *rhs) {
    if (!lhs || !rhs) {
        return 0;
    }
    return strcasecmp(lhs, rhs) == 0;
}

static void datalab_recent_input_roots_compact(char paths[][DATALAB_APP_PATH_CAP],
                                               size_t *io_count,
                                               size_t path_capacity) {
    size_t count = 0u;
    size_t write_idx = 0u;
    size_t read_idx = 0u;
    if (!paths || !io_count || path_capacity == 0u) {
        return;
    }
    count = *io_count;
    if (count > path_capacity) {
        count = path_capacity;
    }
    for (read_idx = 0u; read_idx < count; ++read_idx) {
        size_t prior_idx = 0u;
        int duplicate = 0;
        datalab_normalize_input_root_path(paths[read_idx], DATALAB_APP_PATH_CAP);
        if (paths[read_idx][0] == '\0') {
            continue;
        }
        for (prior_idx = 0u; prior_idx < write_idx; ++prior_idx) {
            if (datalab_input_root_path_equals(paths[prior_idx], paths[read_idx])) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        if (write_idx != read_idx) {
            snprintf(paths[write_idx], DATALAB_APP_PATH_CAP, "%s", paths[read_idx]);
        }
        write_idx++;
    }
    *io_count = write_idx;
}

static DatalabWorkspaceCustomTheme datalab_default_custom_theme(void) {
    return (DatalabWorkspaceCustomTheme){
        12, 14, 20,
        54, 36, 74,
        24, 28, 38,
        112, 124, 146,
        226, 234, 246,
        178, 194, 220,
        34, 40, 58,
        48, 58, 84,
        116, 136, 184
    };
}

static void datalab_default_custom_theme_name(int slot_index, char *out_name, size_t out_cap) {
    if (!out_name || out_cap == 0u) {
        return;
    }
    (void)snprintf(out_name, out_cap, "custom_%d", slot_index + 1);
}

static void datalab_sanitize_custom_theme_name(char *name, size_t cap, int slot_index) {
    size_t i = 0u;
    if (!name || cap == 0u) {
        return;
    }
    name[cap - 1u] = '\0';
    for (i = 0u; name[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == ' ') {
            continue;
        }
        name[i] = '_';
    }
    if (name[0] == '\0') {
        datalab_default_custom_theme_name(slot_index, name, cap);
    }
}

static int datalab_scan_custom_theme(FILE *fp, DatalabWorkspaceCustomTheme *out_theme) {
    int values[27];
    int i = 0;
    int matched = 0;
    DatalabWorkspaceCustomTheme parsed;

    if (!fp || !out_theme) {
        return 0;
    }
    for (i = 0; i < 27; ++i) {
        values[i] = 0;
    }
    matched = fscanf(fp,
                     "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                     &values[0], &values[1], &values[2],
                     &values[3], &values[4], &values[5],
                     &values[6], &values[7], &values[8],
                     &values[9], &values[10], &values[11],
                     &values[12], &values[13], &values[14],
                     &values[15], &values[16], &values[17],
                     &values[18], &values[19], &values[20],
                     &values[21], &values[22], &values[23],
                     &values[24], &values[25], &values[26]);
    if (matched != 27) {
        return 0;
    }
#define DATALAB_CLAMP_BYTE(v) ((uint8_t)(((v) < 0) ? 0 : (((v) > 255) ? 255 : (v))))
    parsed = (DatalabWorkspaceCustomTheme){
        DATALAB_CLAMP_BYTE(values[0]), DATALAB_CLAMP_BYTE(values[1]), DATALAB_CLAMP_BYTE(values[2]),
        DATALAB_CLAMP_BYTE(values[3]), DATALAB_CLAMP_BYTE(values[4]), DATALAB_CLAMP_BYTE(values[5]),
        DATALAB_CLAMP_BYTE(values[6]), DATALAB_CLAMP_BYTE(values[7]), DATALAB_CLAMP_BYTE(values[8]),
        DATALAB_CLAMP_BYTE(values[9]), DATALAB_CLAMP_BYTE(values[10]), DATALAB_CLAMP_BYTE(values[11]),
        DATALAB_CLAMP_BYTE(values[12]), DATALAB_CLAMP_BYTE(values[13]), DATALAB_CLAMP_BYTE(values[14]),
        DATALAB_CLAMP_BYTE(values[15]), DATALAB_CLAMP_BYTE(values[16]), DATALAB_CLAMP_BYTE(values[17]),
        DATALAB_CLAMP_BYTE(values[18]), DATALAB_CLAMP_BYTE(values[19]), DATALAB_CLAMP_BYTE(values[20]),
        DATALAB_CLAMP_BYTE(values[21]), DATALAB_CLAMP_BYTE(values[22]), DATALAB_CLAMP_BYTE(values[23]),
        DATALAB_CLAMP_BYTE(values[24]), DATALAB_CLAMP_BYTE(values[25]), DATALAB_CLAMP_BYTE(values[26])
    };
#undef DATALAB_CLAMP_BYTE
    *out_theme = parsed;
    return 1;
}

static void datalab_write_custom_theme(FILE *fp, const DatalabWorkspaceCustomTheme *theme, int trailing_newline) {
    DatalabWorkspaceCustomTheme t;
    if (!fp) {
        return;
    }
    t = theme ? *theme : datalab_default_custom_theme();
    (void)fprintf(fp,
                  "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u%s",
                  (unsigned int)t.clear_r, (unsigned int)t.clear_g, (unsigned int)t.clear_b,
                  (unsigned int)t.pane_fill_r, (unsigned int)t.pane_fill_g, (unsigned int)t.pane_fill_b,
                  (unsigned int)t.shell_fill_r, (unsigned int)t.shell_fill_g, (unsigned int)t.shell_fill_b,
                  (unsigned int)t.shell_border_r, (unsigned int)t.shell_border_g, (unsigned int)t.shell_border_b,
                  (unsigned int)t.text_primary_r, (unsigned int)t.text_primary_g, (unsigned int)t.text_primary_b,
                  (unsigned int)t.text_secondary_r, (unsigned int)t.text_secondary_g, (unsigned int)t.text_secondary_b,
                  (unsigned int)t.button_fill_r, (unsigned int)t.button_fill_g, (unsigned int)t.button_fill_b,
                  (unsigned int)t.button_hover_r, (unsigned int)t.button_hover_g, (unsigned int)t.button_hover_b,
                  (unsigned int)t.button_active_r, (unsigned int)t.button_active_g, (unsigned int)t.button_active_b,
                  trailing_newline ? "\n" : "");
}

static int datalab_ensure_runtime_dirs(void) {
    if (mkdir("data", 0777) != 0 && errno != EEXIST) {
        return 0;
    }
    if (mkdir("data/runtime", 0777) != 0 && errno != EEXIST) {
        return 0;
    }
    return 1;
}

const char *datalab_runtime_prefs_last_diagnostic(void) {
    return g_datalab_runtime_prefs_diagnostic;
}

void datalab_runtime_prefs_clear_diagnostic(void) {
    g_datalab_runtime_prefs_diagnostic[0] = '\0';
}

void datalab_normalize_input_root_path(char *path, size_t path_cap) {
    size_t len = 0u;
    if (!path || path_cap == 0u) {
        return;
    }
    path[path_cap - 1u] = '\0';
    len = strlen(path);
    while (len > 1u && path[len - 1u] == '/') {
        path[len - 1u] = '\0';
        len--;
    }
}

void datalab_recent_input_roots_add(char paths[][DATALAB_APP_PATH_CAP],
                                    size_t *io_count,
                                    size_t path_capacity,
                                    const char *path) {
    size_t count = 0u;
    size_t i = 0u;
    size_t existing_idx = 0u;
    char normalized[DATALAB_APP_PATH_CAP];
    if (!paths || !io_count || path_capacity == 0u || !path || path[0] == '\0') {
        return;
    }
    snprintf(normalized, sizeof(normalized), "%s", path);
    datalab_normalize_input_root_path(normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return;
    }
    datalab_recent_input_roots_compact(paths, io_count, path_capacity);
    count = *io_count;
    if (count > path_capacity) {
        count = path_capacity;
    }
    for (i = 0u; i < count; ++i) {
        if (datalab_input_root_path_equals(paths[i], normalized)) {
            existing_idx = i;
            break;
        }
    }

    /*
     * Move only the prefix ahead of an existing entry.  Moving every entry
     * would retain the old matching path at its shifted position, creating a
     * duplicate after the new first entry is written.
     */
    if (i < count) {
        for (i = existing_idx; i > 0u; --i) {
            snprintf(paths[i], DATALAB_APP_PATH_CAP, "%s", paths[i - 1u]);
        }
    } else {
        const size_t last_idx = count < path_capacity ? count : path_capacity - 1u;
        for (i = last_idx; i > 0u; --i) {
            snprintf(paths[i], DATALAB_APP_PATH_CAP, "%s", paths[i - 1u]);
        }
        if (count < path_capacity) {
            count++;
        }
    }
    snprintf(paths[0], DATALAB_APP_PATH_CAP, "%s", normalized);
    *io_count = count;
}

void datalab_recent_input_files_add(char paths[][DATALAB_APP_PATH_CAP],
                                    size_t *io_count,
                                    size_t path_capacity,
                                    const char *path) {
    datalab_recent_input_roots_add(paths, io_count, path_capacity, path);
}

int datalab_input_root_select_recent(char *io_input_root,
                                     size_t input_root_cap,
                                     char recent_paths[][DATALAB_APP_PATH_CAP],
                                     size_t *io_recent_count,
                                     size_t recent_capacity,
                                     const char *path) {
    char normalized[DATALAB_APP_PATH_CAP];
    if (!io_input_root || input_root_cap == 0u || !recent_paths ||
        !io_recent_count || recent_capacity == 0u || !path || path[0] == '\0') {
        return 0;
    }
    snprintf(normalized, sizeof(normalized), "%s", path);
    datalab_normalize_input_root_path(normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return 0;
    }
    snprintf(io_input_root, input_root_cap, "%s", normalized);
    datalab_normalize_input_root_path(io_input_root, input_root_cap);
    if (io_input_root[0] == '\0') {
        return 0;
    }
    datalab_recent_input_roots_add(recent_paths,
                                   io_recent_count,
                                   recent_capacity,
                                   io_input_root);
    return 1;
}

int datalab_runtime_prefs_load_text_zoom_step(int *out_step) {
    FILE *fp;
    char line[64];
    long parsed;
    char *end = NULL;
    if (!out_step) {
        return 0;
    }
    if (!datalab_runtime_prefs_open_for_load(k_datalab_text_zoom_step_path, &fp)) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_text_zoom_step_path, "missing text zoom step");
        fclose(fp);
        return 0;
    }
    fclose(fp);
    parsed = strtol(line, &end, 10);
    if (end == line) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_text_zoom_step_path, "invalid text zoom step");
        return 0;
    }
    *out_step = datalab_text_zoom_step_clamp((int)parsed);
    return 1;
}

int datalab_runtime_prefs_load_input_root(char *out_path, size_t out_cap) {
    FILE *fp;
    char line[DATALAB_APP_PATH_CAP];
    size_t len = 0u;
    if (!out_path || out_cap == 0u) {
        return 0;
    }
    out_path[0] = '\0';
    if (!datalab_runtime_prefs_open_for_load(k_datalab_input_root_path, &fp)) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_input_root_path, "missing input root");
        fclose(fp);
        return 0;
    }
    fclose(fp);
    len = strcspn(line, "\r\n");
    line[len] = '\0';
    datalab_normalize_input_root_path(line, sizeof(line));
    if (line[0] == '\0') {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_input_root_path, "empty input root");
        return 0;
    }
    snprintf(out_path, out_cap, "%s", line);
    return 1;
}

int datalab_runtime_prefs_load_recent_input_roots(char out_paths[][DATALAB_APP_PATH_CAP],
                                                  size_t path_capacity,
                                                  size_t *out_count) {
    FILE *fp = NULL;
    char line[DATALAB_APP_PATH_CAP];
    size_t count = 0u;
    if (!out_paths || path_capacity == 0u || !out_count) {
        return 0;
    }
    *out_count = 0u;
    if (!datalab_runtime_prefs_open_for_load(k_datalab_recent_input_roots_path, &fp)) {
        return 0;
    }
    while (count < path_capacity && fgets(line, sizeof(line), fp)) {
        size_t len = strcspn(line, "\r\n");
        line[len] = '\0';
        datalab_normalize_input_root_path(line, sizeof(line));
        if (line[0] == '\0') {
            continue;
        }
        snprintf(out_paths[count], DATALAB_APP_PATH_CAP, "%s", line);
        count++;
    }
    fclose(fp);
    *out_count = count;
    datalab_recent_input_roots_compact(out_paths, out_count, path_capacity);
    if (*out_count == 0u) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_recent_input_roots_path, "no valid recent input roots");
    }
    return *out_count > 0u;
}

int datalab_runtime_prefs_load_recent_input_files(char out_paths[][DATALAB_APP_PATH_CAP],
                                                  size_t path_capacity,
                                                  size_t *out_count) {
    FILE *fp = NULL;
    char line[DATALAB_APP_PATH_CAP];
    size_t count = 0u;
    if (!out_paths || path_capacity == 0u || !out_count) {
        return 0;
    }
    *out_count = 0u;
    if (!datalab_runtime_prefs_open_for_load(k_datalab_recent_input_files_path, &fp)) {
        return 0;
    }
    while (count < path_capacity && fgets(line, sizeof(line), fp)) {
        size_t len = strcspn(line, "\r\n");
        line[len] = '\0';
        if (line[0] == '\0') {
            continue;
        }
        snprintf(out_paths[count], DATALAB_APP_PATH_CAP, "%s", line);
        count++;
    }
    fclose(fp);
    *out_count = count;
    datalab_recent_input_roots_compact(out_paths, out_count, path_capacity);
    return *out_count > 0u;
}

int datalab_runtime_prefs_load_pinned_input_files(char out_paths[][DATALAB_APP_PATH_CAP], size_t path_capacity, size_t *out_count) {
    FILE *fp = NULL; char line[DATALAB_APP_PATH_CAP]; size_t count = 0u;
    if (!out_paths || path_capacity == 0u || !out_count) return 0;
    *out_count = 0u;
    if (!datalab_runtime_prefs_open_for_load(k_datalab_pinned_input_files_path, &fp)) return 0;
    while (count < path_capacity && fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0') snprintf(out_paths[count++], DATALAB_APP_PATH_CAP, "%s", line);
    }
    fclose(fp); *out_count = count;
    datalab_recent_input_roots_compact(out_paths, out_count, path_capacity);
    return *out_count > 0u;
}

int datalab_runtime_prefs_load_theme_preset_id(uint8_t *out_theme_preset_id) {
    FILE *fp;
    char line[64];
    long parsed = 0;
    char *end = NULL;
    if (!out_theme_preset_id) {
        return 0;
    }
    if (!datalab_runtime_prefs_open_for_load(k_datalab_theme_preset_path, &fp)) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_theme_preset_path, "missing theme preset id");
        fclose(fp);
        return 0;
    }
    fclose(fp);
    parsed = strtol(line, &end, 10);
    if (end == line) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_theme_preset_path, "invalid theme preset id");
        return 0;
    }
    *out_theme_preset_id = datalab_theme_preset_clamp((int)parsed);
    return 1;
}

int datalab_runtime_prefs_load_custom_theme(DatalabWorkspaceCustomTheme *out_theme) {
    FILE *fp = NULL;
    if (!out_theme) {
        return 0;
    }
    if (!datalab_runtime_prefs_open_for_load(k_datalab_custom_theme_path, &fp)) {
        return 0;
    }
    if (!datalab_scan_custom_theme(fp, out_theme)) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_custom_theme_path, "invalid custom theme");
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

int datalab_runtime_prefs_load_custom_theme_slots(DatalabWorkspaceCustomTheme *out_slots, size_t slot_count) {
    FILE *fp = NULL;
    size_t i = 0u;
    if (!out_slots || slot_count < DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return 0;
    }
    if (!datalab_runtime_prefs_open_for_load(k_datalab_custom_theme_slots_path, &fp)) {
        return 0;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        if (!datalab_scan_custom_theme(fp, &out_slots[i])) {
            datalab_runtime_prefs_set_diagnostic("load", k_datalab_custom_theme_slots_path, "invalid custom theme slots");
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return 1;
}

int datalab_runtime_prefs_load_custom_theme_slot_names(char out_names[][DATALAB_CUSTOM_THEME_NAME_CAP],
                                                       size_t slot_count) {
    FILE *fp = NULL;
    size_t i = 0u;
    if (!out_names || slot_count < DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return 0;
    }
    if (!datalab_runtime_prefs_open_for_load(k_datalab_custom_theme_slot_names_path, &fp)) {
        return 0;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        if (!fgets(out_names[i], DATALAB_CUSTOM_THEME_NAME_CAP, fp)) {
            datalab_runtime_prefs_set_diagnostic("load", k_datalab_custom_theme_slot_names_path, "missing custom theme slot name");
            fclose(fp);
            return 0;
        }
        out_names[i][strcspn(out_names[i], "\r\n")] = '\0';
        datalab_sanitize_custom_theme_name(out_names[i], DATALAB_CUSTOM_THEME_NAME_CAP, (int)i);
    }
    fclose(fp);
    return 1;
}

int datalab_runtime_prefs_load_custom_theme_active_slot(uint8_t *out_slot) {
    FILE *fp = NULL;
    char line[64];
    long parsed = 0;
    char *end = NULL;
    if (!out_slot) {
        return 0;
    }
    if (!datalab_runtime_prefs_open_for_load(k_datalab_custom_theme_active_slot_path, &fp)) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_custom_theme_active_slot_path, "missing custom theme active slot");
        fclose(fp);
        return 0;
    }
    fclose(fp);
    parsed = strtol(line, &end, 10);
    if (end == line) {
        datalab_runtime_prefs_set_diagnostic("load", k_datalab_custom_theme_active_slot_path, "invalid custom theme active slot");
        return 0;
    }
    *out_slot = datalab_custom_theme_slot_clamp((int)parsed);
    return 1;
}

int datalab_runtime_prefs_save_text_zoom_step(int step) {
    FILE *fp = datalab_runtime_prefs_open_for_save(k_datalab_text_zoom_step_path);
    if (!fp) {
        return 0;
    }
    if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%d\n", datalab_text_zoom_step_clamp(step)),
                                        fp,
                                        k_datalab_text_zoom_step_path)) {
        fclose(fp);
        return 0;
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_text_zoom_step_path);
}

int datalab_runtime_prefs_save_input_root(const char *path) {
    FILE *fp = NULL;
    char normalized[DATALAB_APP_PATH_CAP];
    if (!path || path[0] == '\0') {
        return 0;
    }
    snprintf(normalized, sizeof(normalized), "%s", path);
    datalab_normalize_input_root_path(normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return 0;
    }
    fp = datalab_runtime_prefs_open_for_save(k_datalab_input_root_path);
    if (!fp) {
        return 0;
    }
    if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%s\n", normalized), fp, k_datalab_input_root_path)) {
        fclose(fp);
        return 0;
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_input_root_path);
}

int datalab_runtime_prefs_save_recent_input_roots(const char paths[][DATALAB_APP_PATH_CAP], size_t count) {
    FILE *fp = NULL;
    char normalized_paths[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP] = {{0}};
    size_t i = 0u;
    size_t normalized_count = 0u;
    if (!paths || count == 0u) {
        return 0;
    }
    fp = datalab_runtime_prefs_open_for_save(k_datalab_recent_input_roots_path);
    if (!fp) {
        return 0;
    }
    if (count > DATALAB_RECENT_INPUT_ROOT_LIMIT) {
        count = DATALAB_RECENT_INPUT_ROOT_LIMIT;
    }
    for (i = 0u; i < count; ++i) {
        snprintf(normalized_paths[i], DATALAB_APP_PATH_CAP, "%s", paths[i]);
    }
    normalized_count = count;
    /* Preserve MRU order while normalizing and removing stale duplicates. */
    datalab_recent_input_roots_compact(normalized_paths,
                                       &normalized_count,
                                       DATALAB_RECENT_INPUT_ROOT_LIMIT);
    for (i = 0u; i < normalized_count; ++i) {
        if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%s\n", normalized_paths[i]),
                                            fp,
                                            k_datalab_recent_input_roots_path)) {
            fclose(fp);
            return 0;
        }
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_recent_input_roots_path);
}

int datalab_runtime_prefs_save_recent_input_files(const char paths[][DATALAB_APP_PATH_CAP], size_t count) {
    FILE *fp = NULL;
    char normalized_paths[DATALAB_RECENT_INPUT_FILE_LIMIT][DATALAB_APP_PATH_CAP] = {{0}};
    size_t i = 0u;
    size_t normalized_count = 0u;
    if (!paths || count == 0u) {
        return 0;
    }
    fp = datalab_runtime_prefs_open_for_save(k_datalab_recent_input_files_path);
    if (!fp) {
        return 0;
    }
    if (count > DATALAB_RECENT_INPUT_FILE_LIMIT) {
        count = DATALAB_RECENT_INPUT_FILE_LIMIT;
    }
    for (i = 0u; i < count; ++i) {
        snprintf(normalized_paths[i], DATALAB_APP_PATH_CAP, "%s", paths[i]);
    }
    normalized_count = count;
    datalab_recent_input_roots_compact(normalized_paths,
                                       &normalized_count,
                                       DATALAB_RECENT_INPUT_FILE_LIMIT);
    for (i = 0u; i < normalized_count; ++i) {
        if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%s\n", normalized_paths[i]),
                                            fp,
                                            k_datalab_recent_input_files_path)) {
            fclose(fp);
            return 0;
        }
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_recent_input_files_path);
}

int datalab_runtime_prefs_save_pinned_input_files(const char paths[][DATALAB_APP_PATH_CAP], size_t count) {
    FILE *fp = NULL; char normalized[DATALAB_RECENT_INPUT_FILE_LIMIT][DATALAB_APP_PATH_CAP] = {{0}}; size_t n = 0u;
    if (!paths) return 0;
    fp = datalab_runtime_prefs_open_for_save(k_datalab_pinned_input_files_path);
    if (!fp) return 0;
    if (count > DATALAB_RECENT_INPUT_FILE_LIMIT) count = DATALAB_RECENT_INPUT_FILE_LIMIT;
    for (size_t i = 0u; i < count; ++i) datalab_recent_input_files_add(normalized, &n, DATALAB_RECENT_INPUT_FILE_LIMIT, paths[i]);
    for (size_t i = 0u; i < n; ++i) if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%s\n", normalized[i]), fp, k_datalab_pinned_input_files_path)) { fclose(fp); return 0; }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_pinned_input_files_path);
}

int datalab_runtime_prefs_save_theme_preset_id(uint8_t theme_preset_id) {
    FILE *fp = datalab_runtime_prefs_open_for_save(k_datalab_theme_preset_path);
    if (!fp) {
        return 0;
    }
    if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%u\n", (unsigned int)datalab_theme_preset_clamp((int)theme_preset_id)),
                                        fp,
                                        k_datalab_theme_preset_path)) {
        fclose(fp);
        return 0;
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_theme_preset_path);
}

int datalab_runtime_prefs_save_custom_theme(const DatalabWorkspaceCustomTheme *theme) {
    FILE *fp = datalab_runtime_prefs_open_for_save(k_datalab_custom_theme_path);
    if (!fp) {
        return 0;
    }
    datalab_write_custom_theme(fp, theme, 1);
    if (ferror(fp)) {
        datalab_runtime_prefs_set_diagnostic("save", k_datalab_custom_theme_path, "write failed");
        fclose(fp);
        return 0;
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_custom_theme_path);
}

int datalab_runtime_prefs_save_custom_theme_slots(const DatalabWorkspaceCustomTheme *slots, size_t slot_count) {
    FILE *fp = NULL;
    size_t i = 0u;
    if (!slots || slot_count < DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return 0;
    }
    fp = datalab_runtime_prefs_open_for_save(k_datalab_custom_theme_slots_path);
    if (!fp) {
        return 0;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        datalab_write_custom_theme(fp, &slots[i], 1);
        if (ferror(fp)) {
            datalab_runtime_prefs_set_diagnostic("save", k_datalab_custom_theme_slots_path, "write failed");
            fclose(fp);
            return 0;
        }
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_custom_theme_slots_path);
}

int datalab_runtime_prefs_save_custom_theme_slot_names(const char names[][DATALAB_CUSTOM_THEME_NAME_CAP],
                                                       size_t slot_count) {
    FILE *fp = NULL;
    size_t i = 0u;
    char sanitized[DATALAB_CUSTOM_THEME_NAME_CAP];
    if (!names || slot_count < DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return 0;
    }
    fp = datalab_runtime_prefs_open_for_save(k_datalab_custom_theme_slot_names_path);
    if (!fp) {
        return 0;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        if (names[i][0] == '\0') {
            datalab_default_custom_theme_name((int)i, sanitized, sizeof(sanitized));
        } else {
            (void)snprintf(sanitized, sizeof(sanitized), "%s", names[i]);
            datalab_sanitize_custom_theme_name(sanitized, sizeof(sanitized), (int)i);
        }
        if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%s\n", sanitized),
                                            fp,
                                            k_datalab_custom_theme_slot_names_path)) {
            fclose(fp);
            return 0;
        }
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_custom_theme_slot_names_path);
}

int datalab_runtime_prefs_save_custom_theme_active_slot(uint8_t slot) {
    FILE *fp = NULL;
    fp = datalab_runtime_prefs_open_for_save(k_datalab_custom_theme_active_slot_path);
    if (!fp) {
        return 0;
    }
    if (!datalab_runtime_prefs_write_ok(fprintf(fp, "%u\n", (unsigned int)datalab_custom_theme_slot_clamp((int)slot)),
                                        fp,
                                        k_datalab_custom_theme_active_slot_path)) {
        fclose(fp);
        return 0;
    }
    return datalab_runtime_prefs_close_saved(fp, k_datalab_custom_theme_active_slot_path);
}
