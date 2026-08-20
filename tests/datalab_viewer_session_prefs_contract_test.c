#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/datalab_viewer_session_prefs.h"

static int assert_true(int condition, const char *message) {
    if (!condition) fprintf(stderr, "viewer-session contract: %s\n", message);
    return condition;
}

static int enter_temp_root(char *out_previous_cwd, size_t previous_cap) {
    char root[] = "/tmp/datalab-viewer-session-XXXXXX";
    if (!out_previous_cwd || previous_cap == 0u || !getcwd(out_previous_cwd, previous_cap)) return 0;
    return mkdtemp(root) != NULL && chdir(root) == 0;
}

static int test_atomic_round_trip(void) {
    DatalabViewerSession saved;
    DatalabViewerSession loaded;
    DatalabRasterViewportState viewport;
    char previous_cwd[PATH_MAX];
    if (!enter_temp_root(previous_cwd, sizeof(previous_cwd))) return 0;
    datalab_raster_viewport_state_init(&viewport);
    datalab_raster_viewport_sync_state(&viewport, 800, 600, 200u, 100u);
    viewport.fit_mode = 0;
    viewport.reset_requested = 0;
    viewport.viewport.zoom = 2.5f;
    viewport.viewport.pan_x = 33.0f;
    viewport.viewport.pan_y = -17.0f;
    datalab_viewer_session_capture(&saved,
                                   "/tmp/frames/frame_0007.png",
                                   &viewport,
                                   1,
                                   DATALAB_PLAYBACK_MODE_BOUNCE,
                                   4,
                                   1,
                                   DATALAB_SAMPLING_MODE_LINEAR);
    if (!assert_true(datalab_viewer_session_save(&saved), "save should atomically publish a valid snapshot") ||
        !assert_true(access("data/runtime/viewer_session_v1.txt.tmp", F_OK) != 0, "temporary snapshot must not remain") ||
        !assert_true(datalab_viewer_session_load(&loaded), "saved snapshot should load")) {
        (void)chdir(previous_cwd);
        return 0;
    }
    (void)chdir(previous_cwd);
    return assert_true(loaded.schema_valid && loaded.selected_path_present &&
                           strcmp(loaded.selected_path, "/tmp/frames/frame_0007.png") == 0,
                       "selected path should round-trip") &&
           assert_true(loaded.fit_mode_present && !loaded.fit_mode && loaded.zoom_present &&
                           loaded.pan_x_present && loaded.pan_y_present && loaded.zoom == 2.5f &&
                           loaded.pan_x == 33.0f && loaded.pan_y == -17.0f,
                       "free viewport should round-trip") &&
           assert_true(loaded.playback_active_present && loaded.playback_active &&
                           loaded.playback_mode == DATALAB_PLAYBACK_MODE_BOUNCE &&
                           loaded.playback_speed_index == 4 && loaded.hud_collapsed &&
                           loaded.sampling_mode == DATALAB_SAMPLING_MODE_LINEAR,
                       "playback, HUD, and forward sampling intent should round-trip");
}

static int test_independent_field_fallback(void) {
    DatalabViewerSession session;
    DatalabRasterViewportState viewport;
    int playback_active = 0;
    int playback_speed = DATALAB_PLAYBACK_SPEED_INDEX_DEFAULT;
    uint32_t interval = DATALAB_PLAYBACK_INTERVAL_MS_DEFAULT;
    int hud_collapsed = 0;
    DatalabSamplingMode sampling = DATALAB_SAMPLING_MODE_DEFAULT;
    char previous_cwd[PATH_MAX];
    FILE *fp = NULL;
    if (!enter_temp_root(previous_cwd, sizeof(previous_cwd))) return 0;
    if (mkdir("data", 0777) != 0 || mkdir("data/runtime", 0777) != 0 ||
        !(fp = fopen("data/runtime/viewer_session_v1.txt", "wb"))) {
        (void)chdir(previous_cwd);
        return 0;
    }
    fputs("schema_version=1\nselected_path=/tmp/frame.png\nfit_mode=free\nzoom=nan\npan_x=12\npan_y=-9\nplayback_active=1\nplayback_mode=bounce\nplayback_speed_index=4\nhud_collapsed=1\nsampling_mode=unsupported\nunknown_future=value\n", fp);
    fclose(fp);
    datalab_raster_viewport_state_init(&viewport);
    datalab_raster_viewport_sync_state(&viewport, 640, 480, 100u, 50u);
    if (!assert_true(datalab_viewer_session_load(&session), "schema-valid partial snapshot should load") ||
        !assert_true(!session.zoom_present && session.pan_x_present && session.pan_y_present &&
                         !session.sampling_mode_present,
                     "malformed fields should fail independently") ) {
        (void)chdir(previous_cwd);
        return 0;
    }
    datalab_viewer_session_apply_presentation(&session,
                                              DATALAB_PROFILE_IMAGE,
                                              100u,
                                              50u,
                                              &viewport,
                                              &playback_active,
                                              &session.playback_mode,
                                              &playback_speed,
                                              &interval,
                                              &hud_collapsed,
                                              &sampling);
    (void)chdir(previous_cwd);
    return assert_true(viewport.fit_mode && viewport.reset_requested == 0,
                       "incomplete free viewport should retain initialized fit state") &&
           assert_true(playback_active && session.playback_mode == DATALAB_PLAYBACK_MODE_BOUNCE &&
                           playback_speed == 4 && interval == datalab_playback_interval_for_speed_index(4) &&
                           hud_collapsed && sampling == DATALAB_SAMPLING_MODE_DEFAULT,
                       "valid fields should restore while unknown sampling retains its default");
}

int main(void) {
    return test_atomic_round_trip() && test_independent_field_fallback() ? 0 : 1;
}
