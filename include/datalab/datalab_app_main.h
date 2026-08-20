#ifndef DATALAB_APP_MAIN_H
#define DATALAB_APP_MAIN_H

#include "app/app_state.h"
#include "app/datalab_async_decode.h"
#include "app/datalab_focus_window.h"
#include "app/datalab_input_catalog.h"
#include "app/datalab_image_residency.h"
#include "app/datalab_viewer_session_prefs.h"
#include "data/pack_loader.h"

typedef struct DatalabAppRuntime {
    const char *argv0;
    const char *pack_path;
    int no_gui;
    int show_help;
    int text_zoom_step;
    uint8_t workspace_authoring_theme_preset_id;
    float workspace_authoring_profile_surface_ratio;
    DatalabWorkspaceCustomTheme workspace_authoring_custom_theme;
    uint8_t workspace_authoring_custom_theme_active_slot;
    DatalabWorkspaceCustomTheme workspace_authoring_custom_theme_slots[DATALAB_CUSTOM_THEME_SLOT_COUNT];
    char workspace_authoring_custom_theme_slot_names[DATALAB_CUSTOM_THEME_SLOT_COUNT][DATALAB_CUSTOM_THEME_NAME_CAP];
    int frame_loaded;
    int input_root_from_cli;
    int reopened_last_input_file;
    int playback_active;
    DatalabPlaybackMode playback_mode;
    int playback_direction;
    int playback_speed_index;
    uint32_t playback_interval_ms;
    int session_hud_collapsed;
    DatalabSamplingMode sampling_mode;
    DatalabRasterViewportState raster_viewport;
    DatalabViewerSession viewer_session;
    int viewer_session_restore_pending;
    char input_root[DATALAB_APP_PATH_CAP];
    char recent_input_roots[DATALAB_RECENT_INPUT_ROOT_LIMIT][DATALAB_APP_PATH_CAP];
    size_t recent_input_root_count;
    char selected_pack_path[DATALAB_APP_PATH_CAP];
    char visual_artifact_path[DATALAB_APP_PATH_CAP];
    char last_load_error[256];
    DatalabInputCatalog input_catalog;
    DatalabAsyncDecode async_decode;
    /* W4 owns only bounded catalog-index intents. Pixels remain in the
     * residency manager and textures remain render-thread-owned. */
    DatalabFocusWindow focus_window;
    /* Heap-owned so image residency does not enlarge this long-lived runtime
     * object or test stack frames. */
    DatalabImageResidency *image_residency;
    uint64_t raster_content_generation;
    DatalabFrame frame;
} DatalabAppRuntime;

void datalab_app_runtime_init(DatalabAppRuntime *runtime);

int datalab_app_main(int argc, char **argv);
int datalab_app_main_legacy(int argc, char **argv);

int datalab_app_bootstrap(int argc, char **argv, DatalabAppRuntime *runtime);
int datalab_app_config_load(DatalabAppRuntime *runtime);
int datalab_app_state_seed(DatalabAppRuntime *runtime);
int datalab_app_subsystems_init(DatalabAppRuntime *runtime, DatalabAppState *app_state);
int datalab_runtime_start(DatalabAppRuntime *runtime, DatalabAppState *app_state);
int datalab_app_run_loop(DatalabAppRuntime *runtime);
void datalab_app_shutdown(DatalabAppRuntime *runtime);

#endif
