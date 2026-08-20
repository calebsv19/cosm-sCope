#include "app/datalab_viewer_session_prefs.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *k_datalab_viewer_session_path = "data/runtime/viewer_session_v1.txt";
static const char *k_datalab_viewer_session_temp_path = "data/runtime/viewer_session_v1.txt.tmp";

static int datalab_viewer_session_ensure_runtime_dirs(void) {
    return (mkdir("data", 0777) == 0 || errno == EEXIST) &&
           (mkdir("data/runtime", 0777) == 0 || errno == EEXIST);
}

static int datalab_viewer_session_parse_bool(const char *text, int *out_value) {
    if (!text || !out_value || (strcmp(text, "0") != 0 && strcmp(text, "1") != 0)) {
        return 0;
    }
    *out_value = text[0] == '1';
    return 1;
}

static int datalab_viewer_session_parse_float(const char *text, float *out_value) {
    char *end = NULL;
    float value = 0.0f;
    if (!text || !out_value) {
        return 0;
    }
    errno = 0;
    value = strtof(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(value)) {
        return 0;
    }
    *out_value = value;
    return 1;
}

static int datalab_viewer_session_parse_int(const char *text, int minimum, int maximum, int *out_value) {
    char *end = NULL;
    long value = 0;
    if (!text || !out_value) {
        return 0;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < minimum || value > maximum) {
        return 0;
    }
    *out_value = (int)value;
    return 1;
}

static int datalab_viewer_session_write_line(FILE *fp, const char *key, const char *value) {
    return fp && key && value && fprintf(fp, "%s=%s\n", key, value) >= 0 && !ferror(fp);
}

static const char *datalab_viewer_session_sampling_name(DatalabSamplingMode mode) {
    switch (mode) {
        case DATALAB_SAMPLING_MODE_NEAREST: return "nearest";
        case DATALAB_SAMPLING_MODE_LINEAR: return "linear";
        case DATALAB_SAMPLING_MODE_DEFAULT:
        default: return "default";
    }
}

static int datalab_viewer_session_parse_sampling(const char *text, DatalabSamplingMode *out_mode) {
    if (!text || !out_mode) return 0;
    if (strcmp(text, "default") == 0) *out_mode = DATALAB_SAMPLING_MODE_DEFAULT;
    else if (strcmp(text, "nearest") == 0) *out_mode = DATALAB_SAMPLING_MODE_NEAREST;
    else if (strcmp(text, "linear") == 0) *out_mode = DATALAB_SAMPLING_MODE_LINEAR;
    else return 0;
    return 1;
}

void datalab_viewer_session_init(DatalabViewerSession *session) {
    if (session) memset(session, 0, sizeof(*session));
}

int datalab_viewer_session_load(DatalabViewerSession *out_session) {
    FILE *fp = NULL;
    DatalabViewerSession parsed;
    char line[DATALAB_APP_PATH_CAP + 64u];
    int version_seen = 0;
    int version_valid = 0;
    if (!out_session) return 0;
    datalab_viewer_session_init(out_session);
    fp = fopen(k_datalab_viewer_session_path, "rb");
    if (!fp) return 0;
    datalab_viewer_session_init(&parsed);
    while (fgets(line, sizeof(line), fp)) {
        char *equals = NULL;
        char *key = line;
        char *value = NULL;
        line[strcspn(line, "\r\n")] = '\0';
        equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';
        value = equals + 1;
        if (strcmp(key, "schema_version") == 0) {
            int version = 0;
            version_seen = 1;
            version_valid = datalab_viewer_session_parse_int(value, 1, 1, &version);
        } else if (strcmp(key, "selected_path") == 0) {
            if (value[0] != '\0' && strlen(value) < sizeof(parsed.selected_path)) {
                snprintf(parsed.selected_path, sizeof(parsed.selected_path), "%s", value);
                parsed.selected_path_present = 1;
            }
        } else if (strcmp(key, "fit_mode") == 0) {
            parsed.fit_mode_present = strcmp(value, "fit") == 0 || strcmp(value, "free") == 0;
            if (parsed.fit_mode_present) parsed.fit_mode = strcmp(value, "fit") == 0;
        } else if (strcmp(key, "zoom") == 0) {
            parsed.zoom_present = datalab_viewer_session_parse_float(value, &parsed.zoom);
        } else if (strcmp(key, "pan_x") == 0) {
            parsed.pan_x_present = datalab_viewer_session_parse_float(value, &parsed.pan_x);
        } else if (strcmp(key, "pan_y") == 0) {
            parsed.pan_y_present = datalab_viewer_session_parse_float(value, &parsed.pan_y);
        } else if (strcmp(key, "playback_active") == 0) {
            parsed.playback_active_present = datalab_viewer_session_parse_bool(value, &parsed.playback_active);
        } else if (strcmp(key, "playback_mode") == 0) {
            parsed.playback_mode_present = strcmp(value, "loop") == 0 || strcmp(value, "bounce") == 0;
            if (parsed.playback_mode_present) parsed.playback_mode = strcmp(value, "bounce") == 0 ? DATALAB_PLAYBACK_MODE_BOUNCE : DATALAB_PLAYBACK_MODE_LOOP;
        } else if (strcmp(key, "playback_speed_index") == 0) {
            parsed.playback_speed_index_present = datalab_viewer_session_parse_int(value, DATALAB_PLAYBACK_SPEED_INDEX_MIN, DATALAB_PLAYBACK_SPEED_INDEX_MAX, &parsed.playback_speed_index);
        } else if (strcmp(key, "hud_collapsed") == 0) {
            parsed.hud_collapsed_present = datalab_viewer_session_parse_bool(value, &parsed.hud_collapsed);
        } else if (strcmp(key, "sampling_mode") == 0) {
            parsed.sampling_mode_present = datalab_viewer_session_parse_sampling(value, &parsed.sampling_mode);
        }
    }
    if (ferror(fp) || fclose(fp) != 0 || !version_seen || !version_valid) return 0;
    parsed.schema_valid = 1;
    *out_session = parsed;
    return 1;
}

int datalab_viewer_session_save(const DatalabViewerSession *session) {
    FILE *fp = NULL;
    char number[64];
    int ok = 0;
    if (!session || !session->selected_path_present || session->selected_path[0] == '\0' ||
        strpbrk(session->selected_path, "\r\n")) return 0;
    if (!datalab_viewer_session_ensure_runtime_dirs()) return 0;
    (void)remove(k_datalab_viewer_session_temp_path);
    fp = fopen(k_datalab_viewer_session_temp_path, "wb");
    if (!fp) return 0;
    ok = datalab_viewer_session_write_line(fp, "schema_version", "1") &&
         datalab_viewer_session_write_line(fp, "selected_path", session->selected_path) &&
         datalab_viewer_session_write_line(fp, "fit_mode", session->fit_mode ? "fit" : "free");
    (void)snprintf(number, sizeof(number), "%.9g", (double)session->zoom);
    ok = ok && datalab_viewer_session_write_line(fp, "zoom", number);
    (void)snprintf(number, sizeof(number), "%.9g", (double)session->pan_x);
    ok = ok && datalab_viewer_session_write_line(fp, "pan_x", number);
    (void)snprintf(number, sizeof(number), "%.9g", (double)session->pan_y);
    ok = ok && datalab_viewer_session_write_line(fp, "pan_y", number);
    ok = ok && datalab_viewer_session_write_line(fp, "playback_active", session->playback_active ? "1" : "0") &&
         datalab_viewer_session_write_line(fp, "playback_mode", session->playback_mode == DATALAB_PLAYBACK_MODE_BOUNCE ? "bounce" : "loop");
    (void)snprintf(number, sizeof(number), "%d", datalab_playback_speed_index_clamp(session->playback_speed_index));
    ok = ok && datalab_viewer_session_write_line(fp, "playback_speed_index", number) &&
         datalab_viewer_session_write_line(fp, "hud_collapsed", session->hud_collapsed ? "1" : "0") &&
         datalab_viewer_session_write_line(fp, "sampling_mode", datalab_viewer_session_sampling_name(session->sampling_mode));
    if (!ok || fflush(fp) != 0) {
        (void)fclose(fp);
        (void)remove(k_datalab_viewer_session_temp_path);
        return 0;
    }
    if (fclose(fp) != 0) {
        (void)remove(k_datalab_viewer_session_temp_path);
        return 0;
    }
    if (rename(k_datalab_viewer_session_temp_path, k_datalab_viewer_session_path) != 0) {
        (void)remove(k_datalab_viewer_session_temp_path);
        return 0;
    }
    return 1;
}

void datalab_viewer_session_capture(DatalabViewerSession *out_session,
                                    const char *selected_path,
                                    const DatalabRasterViewportState *viewport,
                                    int playback_active,
                                    DatalabPlaybackMode playback_mode,
                                    int playback_speed_index,
                                    int hud_collapsed,
                                    DatalabSamplingMode sampling_mode) {
    if (!out_session) return;
    datalab_viewer_session_init(out_session);
    out_session->schema_valid = 1;
    if (selected_path && selected_path[0] != '\0' && strlen(selected_path) < sizeof(out_session->selected_path) && !strpbrk(selected_path, "\r\n")) {
        snprintf(out_session->selected_path, sizeof(out_session->selected_path), "%s", selected_path);
        out_session->selected_path_present = 1;
    }
    out_session->fit_mode_present = 1;
    out_session->fit_mode = !viewport || viewport->fit_mode;
    out_session->zoom_present = viewport && isfinite(viewport->viewport.zoom);
    out_session->zoom = out_session->zoom_present ? viewport->viewport.zoom : 1.0f;
    out_session->pan_x_present = viewport && isfinite(viewport->viewport.pan_x);
    out_session->pan_x = out_session->pan_x_present ? viewport->viewport.pan_x : 0.0f;
    out_session->pan_y_present = viewport && isfinite(viewport->viewport.pan_y);
    out_session->pan_y = out_session->pan_y_present ? viewport->viewport.pan_y : 0.0f;
    out_session->playback_active_present = 1;
    out_session->playback_active = playback_active ? 1 : 0;
    out_session->playback_mode_present = 1;
    out_session->playback_mode = playback_mode == DATALAB_PLAYBACK_MODE_BOUNCE ? DATALAB_PLAYBACK_MODE_BOUNCE : DATALAB_PLAYBACK_MODE_LOOP;
    out_session->playback_speed_index_present = 1;
    out_session->playback_speed_index = datalab_playback_speed_index_clamp(playback_speed_index);
    out_session->hud_collapsed_present = 1;
    out_session->hud_collapsed = hud_collapsed ? 1 : 0;
    out_session->sampling_mode_present = 1;
    out_session->sampling_mode = sampling_mode;
}

void datalab_viewer_session_apply_presentation(const DatalabViewerSession *session,
                                               DatalabProfile profile,
                                               uint32_t content_width,
                                               uint32_t content_height,
                                               DatalabRasterViewportState *viewport,
                                               int *io_playback_active,
                                               DatalabPlaybackMode *io_playback_mode,
                                               int *io_playback_speed_index,
                                               uint32_t *io_playback_interval_ms,
                                               int *io_hud_collapsed,
                                               DatalabSamplingMode *io_sampling_mode) {
    if (!session || !session->schema_valid) return;
    if (session->playback_active_present && io_playback_active) *io_playback_active = session->playback_active;
    if (session->playback_mode_present && io_playback_mode) *io_playback_mode = session->playback_mode;
    if (session->playback_speed_index_present && io_playback_speed_index) *io_playback_speed_index = datalab_playback_speed_index_clamp(session->playback_speed_index);
    if (io_playback_interval_ms && io_playback_speed_index) *io_playback_interval_ms = datalab_playback_interval_for_speed_index(*io_playback_speed_index);
    if (session->hud_collapsed_present && io_hud_collapsed) *io_hud_collapsed = session->hud_collapsed;
    if (session->sampling_mode_present && io_sampling_mode) *io_sampling_mode = session->sampling_mode;
    if (!viewport || !datalab_profile_supports_raster_viewport(profile) || !session->fit_mode_present) return;
    if (session->fit_mode) {
        datalab_raster_viewport_request_reset(viewport);
        return;
    }
    if (!session->zoom_present || !session->pan_x_present || !session->pan_y_present ||
        !isfinite(session->zoom) || !isfinite(session->pan_x) || !isfinite(session->pan_y) ||
        session->zoom < 0.0001f || session->zoom > 64.0f || content_width == 0u || content_height == 0u) return;
    (void)core_viewport2d_init(&viewport->viewport);
    viewport->viewport.min_zoom = 0.0001f;
    viewport->viewport.max_zoom = 64.0f;
    viewport->viewport.zoom = session->zoom;
    viewport->viewport.pan_x = session->pan_x;
    viewport->viewport.pan_y = session->pan_y;
    viewport->content_width = content_width;
    viewport->content_height = content_height;
    viewport->view_width = 0;
    viewport->view_height = 0;
    viewport->valid = core_viewport2d_validate(&viewport->viewport).code == CORE_OK;
    viewport->fit_mode = 0;
    viewport->reset_requested = viewport->valid ? 0 : 1;
    viewport->drag_active = 0;
}
