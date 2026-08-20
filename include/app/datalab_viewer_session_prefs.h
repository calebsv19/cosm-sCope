#ifndef DATALAB_VIEWER_SESSION_PREFS_H
#define DATALAB_VIEWER_SESSION_PREFS_H

#include "app/app_state.h"

typedef struct DatalabViewerSession {
    int schema_valid;
    int selected_path_present;
    char selected_path[DATALAB_APP_PATH_CAP];
    int fit_mode_present;
    int fit_mode;
    int zoom_present;
    float zoom;
    int pan_x_present;
    float pan_x;
    int pan_y_present;
    float pan_y;
    int playback_active_present;
    int playback_active;
    int playback_mode_present;
    DatalabPlaybackMode playback_mode;
    int playback_speed_index_present;
    int playback_speed_index;
    int hud_collapsed_present;
    int hud_collapsed;
    int sampling_mode_present;
    DatalabSamplingMode sampling_mode;
} DatalabViewerSession;

void datalab_viewer_session_init(DatalabViewerSession *session);
int datalab_viewer_session_load(DatalabViewerSession *out_session);
int datalab_viewer_session_save(const DatalabViewerSession *session);
void datalab_viewer_session_capture(DatalabViewerSession *out_session,
                                    const char *selected_path,
                                    const DatalabRasterViewportState *viewport,
                                    int playback_active,
                                    DatalabPlaybackMode playback_mode,
                                    int playback_speed_index,
                                    int hud_collapsed,
                                    DatalabSamplingMode sampling_mode);
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
                                               DatalabSamplingMode *io_sampling_mode);

#endif
