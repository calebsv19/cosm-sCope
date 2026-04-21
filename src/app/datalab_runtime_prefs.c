#include "app/datalab_runtime_prefs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "datalab/datalab_app_main.h"
#include "render/render_view.h"

static const char *k_datalab_text_zoom_step_path = "data/runtime/text_zoom_step.txt";
static const char *k_datalab_input_root_path = "data/runtime/input_root.txt";
static const char *k_datalab_theme_preset_path = "data/runtime/theme_preset_id.txt";
static const char *k_datalab_custom_theme_path = "data/runtime/custom_theme_v1.txt";
static const char *k_datalab_custom_theme_slots_path = "data/runtime/custom_theme_slots_v1.txt";
static const char *k_datalab_custom_theme_slot_names_path = "data/runtime/custom_theme_slot_names_v1.txt";
static const char *k_datalab_custom_theme_active_slot_path = "data/runtime/custom_theme_active_slot.txt";

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

int datalab_runtime_prefs_load_text_zoom_step(int *out_step) {
    FILE *fp;
    char line[64];
    long parsed;
    char *end = NULL;
    if (!out_step) {
        return 0;
    }
    fp = fopen(k_datalab_text_zoom_step_path, "rb");
    if (!fp) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    parsed = strtol(line, &end, 10);
    if (end == line) {
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
    fp = fopen(k_datalab_input_root_path, "rb");
    if (!fp) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    len = strcspn(line, "\r\n");
    line[len] = '\0';
    if (line[0] == '\0') {
        return 0;
    }
    snprintf(out_path, out_cap, "%s", line);
    return 1;
}

int datalab_runtime_prefs_load_theme_preset_id(uint8_t *out_theme_preset_id) {
    FILE *fp;
    char line[64];
    long parsed = 0;
    char *end = NULL;
    if (!out_theme_preset_id) {
        return 0;
    }
    fp = fopen(k_datalab_theme_preset_path, "rb");
    if (!fp) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    parsed = strtol(line, &end, 10);
    if (end == line) {
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
    fp = fopen(k_datalab_custom_theme_path, "rb");
    if (!fp) {
        return 0;
    }
    if (!datalab_scan_custom_theme(fp, out_theme)) {
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
    fp = fopen(k_datalab_custom_theme_slots_path, "rb");
    if (!fp) {
        return 0;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        if (!datalab_scan_custom_theme(fp, &out_slots[i])) {
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
    fp = fopen(k_datalab_custom_theme_slot_names_path, "rb");
    if (!fp) {
        return 0;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        if (!fgets(out_names[i], DATALAB_CUSTOM_THEME_NAME_CAP, fp)) {
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
    fp = fopen(k_datalab_custom_theme_active_slot_path, "rb");
    if (!fp) {
        return 0;
    }
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    parsed = strtol(line, &end, 10);
    if (end == line) {
        return 0;
    }
    *out_slot = datalab_custom_theme_slot_clamp((int)parsed);
    return 1;
}

void datalab_runtime_prefs_save_text_zoom_step(int step) {
    FILE *fp;
    if (!datalab_ensure_runtime_dirs()) {
        return;
    }
    fp = fopen(k_datalab_text_zoom_step_path, "wb");
    if (!fp) {
        return;
    }
    (void)fprintf(fp, "%d\n", datalab_text_zoom_step_clamp(step));
    fclose(fp);
}

void datalab_runtime_prefs_save_input_root(const char *path) {
    FILE *fp;
    if (!path || path[0] == '\0') {
        return;
    }
    if (!datalab_ensure_runtime_dirs()) {
        return;
    }
    fp = fopen(k_datalab_input_root_path, "wb");
    if (!fp) {
        return;
    }
    (void)fprintf(fp, "%s\n", path);
    fclose(fp);
}

void datalab_runtime_prefs_save_theme_preset_id(uint8_t theme_preset_id) {
    FILE *fp;
    if (!datalab_ensure_runtime_dirs()) {
        return;
    }
    fp = fopen(k_datalab_theme_preset_path, "wb");
    if (!fp) {
        return;
    }
    (void)fprintf(fp, "%u\n", (unsigned int)datalab_theme_preset_clamp((int)theme_preset_id));
    fclose(fp);
}

void datalab_runtime_prefs_save_custom_theme(const DatalabWorkspaceCustomTheme *theme) {
    FILE *fp;
    if (!datalab_ensure_runtime_dirs()) {
        return;
    }
    fp = fopen(k_datalab_custom_theme_path, "wb");
    if (!fp) {
        return;
    }
    datalab_write_custom_theme(fp, theme, 1);
    fclose(fp);
}

void datalab_runtime_prefs_save_custom_theme_slots(const DatalabWorkspaceCustomTheme *slots, size_t slot_count) {
    FILE *fp = NULL;
    size_t i = 0u;
    if (!datalab_ensure_runtime_dirs() || !slots || slot_count < DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return;
    }
    fp = fopen(k_datalab_custom_theme_slots_path, "wb");
    if (!fp) {
        return;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        datalab_write_custom_theme(fp, &slots[i], 1);
    }
    fclose(fp);
}

void datalab_runtime_prefs_save_custom_theme_slot_names(const char names[][DATALAB_CUSTOM_THEME_NAME_CAP],
                                                        size_t slot_count) {
    FILE *fp = NULL;
    size_t i = 0u;
    char sanitized[DATALAB_CUSTOM_THEME_NAME_CAP];
    if (!datalab_ensure_runtime_dirs() || !names || slot_count < DATALAB_CUSTOM_THEME_SLOT_COUNT) {
        return;
    }
    fp = fopen(k_datalab_custom_theme_slot_names_path, "wb");
    if (!fp) {
        return;
    }
    for (i = 0u; i < DATALAB_CUSTOM_THEME_SLOT_COUNT; ++i) {
        if (names[i][0] == '\0') {
            datalab_default_custom_theme_name((int)i, sanitized, sizeof(sanitized));
        } else {
            (void)snprintf(sanitized, sizeof(sanitized), "%s", names[i]);
            datalab_sanitize_custom_theme_name(sanitized, sizeof(sanitized), (int)i);
        }
        (void)fprintf(fp, "%s\n", sanitized);
    }
    fclose(fp);
}

void datalab_runtime_prefs_save_custom_theme_active_slot(uint8_t slot) {
    FILE *fp = NULL;
    if (!datalab_ensure_runtime_dirs()) {
        return;
    }
    fp = fopen(k_datalab_custom_theme_active_slot_path, "wb");
    if (!fp) {
        return;
    }
    (void)fprintf(fp, "%u\n", (unsigned int)datalab_custom_theme_slot_clamp((int)slot));
    fclose(fp);
}
