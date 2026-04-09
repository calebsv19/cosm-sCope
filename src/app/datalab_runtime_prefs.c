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
