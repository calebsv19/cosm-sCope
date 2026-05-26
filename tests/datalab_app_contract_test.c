#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "datalab/datalab_app_main.h"

#ifndef DATALAB_TEST_DEFAULT_PACK
#define DATALAB_TEST_DEFAULT_PACK ""
#endif

static int datalab_test_mkdir_if_needed(const char *path) {
    if (!path || path[0] == '\0') {
        return 0;
    }
    if (mkdir(path, 0777) == 0) {
        return 1;
    }
    return errno == EEXIST;
}

static int datalab_test_write_text_file(const char *path, const char *text) {
    FILE *fp = NULL;
    if (!path || !text) {
        return 0;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    if (fputs(text, fp) < 0) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static int datalab_test_make_temp_dir(char *out_dir, size_t out_cap) {
    char template_path[PATH_MAX];
    char *made = NULL;
    if (!out_dir || out_cap == 0u) {
        return 0;
    }
    snprintf(template_path, sizeof(template_path), "/tmp/datalab_contract_XXXXXX");
    made = mkdtemp(template_path);
    if (!made) {
        return 0;
    }
    snprintf(out_dir, out_cap, "%s", made);
    return 1;
}

static int datalab_test_enter_temp_runtime_root(const char *temp_dir, char *previous_cwd, size_t cwd_cap) {
    char data_dir[PATH_MAX];
    char runtime_dir[PATH_MAX];
    if (!temp_dir || !previous_cwd || cwd_cap == 0u) {
        return 0;
    }
    if (!getcwd(previous_cwd, cwd_cap)) {
        return 0;
    }
    snprintf(data_dir, sizeof(data_dir), "%s/data", temp_dir);
    snprintf(runtime_dir, sizeof(runtime_dir), "%s/data/runtime", temp_dir);
    if (!datalab_test_mkdir_if_needed(temp_dir) ||
        !datalab_test_mkdir_if_needed(data_dir) ||
        !datalab_test_mkdir_if_needed(runtime_dir)) {
        return 0;
    }
    if (chdir(temp_dir) != 0) {
        return 0;
    }
    return 1;
}

static void datalab_test_restore_cwd(const char *cwd) {
    if (cwd && cwd[0] != '\0') {
        (void)chdir(cwd);
    }
}

static int datalab_test_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "contract: %s\n", message ? message : "assertion failed");
        return 0;
    }
    return 1;
}

static int test_headless_requires_pack(void) {
    DatalabAppRuntime runtime;
    char *argv[] = { (char *)"datalab", (char *)"--no-gui" };
    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(datalab_app_bootstrap(2, argv, &runtime) == 0, "bootstrap failed for no-gui case")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for no-gui case")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) != 0, "headless launch without pack should fail")) {
        return 0;
    }
    return 1;
}

static int test_valid_headless_pack_state_seed(const char *default_pack) {
    DatalabAppRuntime runtime;
    char *argv[] = { (char *)"datalab", (char *)"--pack", (char *)default_pack, (char *)"--no-gui" };
    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(default_pack && default_pack[0] != '\0', "default pack path missing")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_bootstrap(4, argv, &runtime) == 0, "bootstrap failed for valid pack")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for valid pack")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) == 0, "state seed failed for valid pack")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    if (!datalab_test_assert(runtime.frame_loaded == 1, "valid pack should load a frame")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    datalab_app_shutdown(&runtime);
    return 1;
}

static int test_selected_pack_path_fallback(const char *default_pack) {
    DatalabAppRuntime runtime;
    char *argv[] = { (char *)"datalab", (char *)"--no-gui" };
    datalab_app_runtime_init(&runtime);
    snprintf(runtime.selected_pack_path, sizeof(runtime.selected_pack_path), "%s", default_pack);
    if (!datalab_test_assert(datalab_app_bootstrap(2, argv, &runtime) == 0, "bootstrap failed for selected-pack fallback")) {
        return 0;
    }
    snprintf(runtime.selected_pack_path, sizeof(runtime.selected_pack_path), "%s", default_pack);
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for selected-pack fallback")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) == 0, "state seed failed for selected-pack fallback")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    if (!datalab_test_assert(runtime.pack_path == runtime.selected_pack_path, "selected-pack fallback should bind pack_path to runtime storage")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    if (!datalab_test_assert(runtime.frame_loaded == 1, "selected-pack fallback should load a frame")) {
        datalab_app_shutdown(&runtime);
        return 0;
    }
    datalab_app_shutdown(&runtime);
    return 1;
}

static int test_unsupported_extension_sets_bounded_error(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char invalid_path[PATH_MAX];
    char *argv[] = { (char *)"datalab", (char *)"--pack", invalid_path, (char *)"--no-gui" };
    snprintf(invalid_path, sizeof(invalid_path), "%s/invalid_input.txt", temp_dir);
    if (!datalab_test_write_text_file(invalid_path, "not a pack\n")) {
        fprintf(stderr, "contract: failed to create invalid input fixture\n");
        return 0;
    }
    datalab_app_runtime_init(&runtime);
    if (!datalab_test_assert(datalab_app_bootstrap(4, argv, &runtime) == 0, "bootstrap failed for invalid extension")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for invalid extension")) {
        return 0;
    }
    if (!datalab_test_assert(datalab_app_state_seed(&runtime) == 2, "unsupported extension should return bounded load error")) {
        return 0;
    }
    if (!datalab_test_assert(strstr(runtime.last_load_error, "unsupported input file extension") != NULL,
                             "unsupported extension should preserve a clear loader error")) {
        return 0;
    }
    return 1;
}

static int test_cli_input_root_precedence(const char *temp_dir) {
    DatalabAppRuntime runtime;
    char previous_cwd[PATH_MAX];
    char persisted_root[PATH_MAX];
    char cli_root[PATH_MAX];
    char persisted_path[PATH_MAX];
    char *argv[] = { (char *)"datalab", (char *)"--input-root", cli_root };
    int ok = 0;

    snprintf(persisted_root, sizeof(persisted_root), "%s/persisted_root", temp_dir);
    snprintf(cli_root, sizeof(cli_root), "%s/cli_root", temp_dir);
    snprintf(persisted_path, sizeof(persisted_path), "%s/data/runtime/input_root.txt", temp_dir);

    if (!datalab_test_enter_temp_runtime_root(temp_dir, previous_cwd, sizeof(previous_cwd))) {
        fprintf(stderr, "contract: failed to enter temp runtime root\n");
        return 0;
    }
    if (!datalab_test_write_text_file(persisted_path, persisted_root)) {
        datalab_test_restore_cwd(previous_cwd);
        fprintf(stderr, "contract: failed to write persisted input root\n");
        return 0;
    }

    datalab_app_runtime_init(&runtime);
    if (datalab_test_assert(datalab_app_bootstrap(3, argv, &runtime) == 0, "bootstrap failed for cli input-root") &&
        datalab_test_assert(runtime.input_root_from_cli == 1, "cli input-root flag should be tracked") &&
        datalab_test_assert(datalab_app_config_load(&runtime) == 0, "config load failed for cli input-root") &&
        datalab_test_assert(strcmp(runtime.input_root, cli_root) == 0, "cli input-root should override persisted root")) {
        ok = 1;
    }

    datalab_test_restore_cwd(previous_cwd);
    return ok;
}

int main(void) {
    char temp_dir[PATH_MAX];
    const char *default_pack = DATALAB_TEST_DEFAULT_PACK;

    if (!datalab_test_make_temp_dir(temp_dir, sizeof(temp_dir))) {
        fprintf(stderr, "contract: failed to create temp dir\n");
        return 1;
    }

    if (!test_headless_requires_pack()) {
        return 1;
    }
    if (!test_valid_headless_pack_state_seed(default_pack)) {
        return 1;
    }
    if (!test_selected_pack_path_fallback(default_pack)) {
        return 1;
    }
    if (!test_unsupported_extension_sets_bounded_error(temp_dir)) {
        return 1;
    }
    if (!test_cli_input_root_precedence(temp_dir)) {
        return 1;
    }

    puts("datalab app contract test passed");
    return 0;
}
