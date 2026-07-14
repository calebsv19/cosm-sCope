#include "platform/datalab_folder_picker.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool write_script(const char *path, const char *body) {
    FILE *file = fopen(path, "w");
    if (!file) return false;
    if (fputs(body, file) == EOF || fclose(file) != 0) return false;
    return chmod(path, 0700) == 0;
}

static bool read_text(const char *path, char *out, size_t out_size) {
    FILE *file = fopen(path, "r");
    size_t count = 0u;
    if (!file || !out || out_size == 0u) return false;
    count = fread(out, 1u, out_size - 1u, file);
    out[count] = '\0';
    (void)fclose(file);
    return true;
}

static bool make_fixture_root(char *template_path, size_t template_path_size, const char *suffix) {
    const char *temp_dir = getenv("TMPDIR");
    if (!temp_dir || !temp_dir[0]) temp_dir = "/tmp";
    return snprintf(template_path, template_path_size, "%s/%s_XXXXXX", temp_dir, suffix) < (int)template_path_size;
}

static bool setup_fixture(char *root, char *zenity, char *kdialog, char *args_log, char *marker) {
    char template[PATH_MAX];
    const char *zenity_script =
        "#!/bin/sh\n"
        "printf '%s\\n' \"$@\" > \"$DATALAB_FOLDER_PICKER_ARGS_LOG\"\n"
        "case \"$DATALAB_FOLDER_PICKER_ZENITY\" in\n"
        " selected) printf '%s\\n' \"$DATALAB_FOLDER_PICKER_SELECTED_PATH\"; exit 0 ;;\n"
        " cancelled) exit 1 ;;\n"
        " unavailable) exit 127 ;;\n"
        " *) exit 2 ;;\n"
        "esac\n";
    const char *kdialog_script =
        "#!/bin/sh\n"
        ": > \"$DATALAB_FOLDER_PICKER_KDIALOG_MARKER\"\n"
        "printf '%s\\n' \"$@\" > \"$DATALAB_FOLDER_PICKER_ARGS_LOG\"\n"
        "printf '%s\\n' \"$DATALAB_FOLDER_PICKER_KDIALOG_PATH\"\n";
    char *created = make_fixture_root(template, sizeof(template), "datalab_folder_picker") ? mkdtemp(template) : NULL;
    if (!created || snprintf(root, PATH_MAX, "%s", created) >= PATH_MAX ||
        snprintf(zenity, PATH_MAX, "%s/zenity", root) >= PATH_MAX ||
        snprintf(kdialog, PATH_MAX, "%s/kdialog", root) >= PATH_MAX ||
        snprintf(args_log, PATH_MAX, "%s/args.log", root) >= PATH_MAX ||
        snprintf(marker, PATH_MAX, "%s/kdialog-ran", root) >= PATH_MAX) return false;
    return write_script(zenity, zenity_script) && write_script(kdialog, kdialog_script);
}

static void remove_fixture(const char *root, const char *zenity, const char *kdialog, const char *args_log, const char *marker) {
    (void)unlink(zenity); (void)unlink(kdialog); (void)unlink(args_log); (void)unlink(marker); (void)rmdir(root);
}

static bool test_zenity_folder_arguments(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], args_log[PATH_MAX], marker[PATH_MAX], output[PATH_MAX], args[2048];
    bool passed = setup_fixture(root, zenity, kdialog, args_log, marker);
    passed = passed && setenv("PATH", root, 1) == 0 && setenv("DATALAB_FOLDER_PICKER_ARGS_LOG", args_log, 1) == 0 &&
             setenv("DATALAB_FOLDER_PICKER_KDIALOG_MARKER", marker, 1) == 0 && setenv("DATALAB_FOLDER_PICKER_ZENITY", "selected", 1) == 0 &&
             setenv("DATALAB_FOLDER_PICKER_SELECTED_PATH", "/tmp/chosen folder", 1) == 0;
    passed = passed && Datalab_FolderPicker_Select("Choose DataLab Input Folder", "/tmp/start folder", output, sizeof(output)) == DATALAB_FOLDER_PICKER_SELECTED &&
             strcmp(output, "/tmp/chosen folder") == 0 && access(marker, F_OK) != 0 && read_text(args_log, args, sizeof(args)) &&
             strstr(args, "--file-selection\n--directory\n--title\nChoose DataLab Input Folder\n--filename\n/tmp/start folder\n") != NULL;
    remove_fixture(root, zenity, kdialog, args_log, marker);
    return passed;
}

static bool test_kdialog_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], args_log[PATH_MAX], marker[PATH_MAX], output[PATH_MAX], args[2048];
    bool passed = setup_fixture(root, zenity, kdialog, args_log, marker);
    passed = passed && setenv("PATH", root, 1) == 0 && setenv("DATALAB_FOLDER_PICKER_ARGS_LOG", args_log, 1) == 0 &&
             setenv("DATALAB_FOLDER_PICKER_KDIALOG_MARKER", marker, 1) == 0 && setenv("DATALAB_FOLDER_PICKER_ZENITY", "unavailable", 1) == 0 &&
             setenv("DATALAB_FOLDER_PICKER_KDIALOG_PATH", "/tmp/kdialog folder", 1) == 0;
    passed = passed && Datalab_FolderPicker_Select("Choose root", "/tmp/start", output, sizeof(output)) == DATALAB_FOLDER_PICKER_SELECTED &&
             strcmp(output, "/tmp/kdialog folder") == 0 && access(marker, F_OK) == 0 && read_text(args_log, args, sizeof(args)) &&
             strstr(args, "--getexistingdirectory\n/tmp/start\n--title\nChoose root\n") != NULL;
    remove_fixture(root, zenity, kdialog, args_log, marker);
    return passed;
}

static bool test_cancel_does_not_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], args_log[PATH_MAX], marker[PATH_MAX], output[PATH_MAX];
    bool passed = setup_fixture(root, zenity, kdialog, args_log, marker);
    passed = passed && setenv("PATH", root, 1) == 0 && setenv("DATALAB_FOLDER_PICKER_ARGS_LOG", args_log, 1) == 0 &&
             setenv("DATALAB_FOLDER_PICKER_KDIALOG_MARKER", marker, 1) == 0 && setenv("DATALAB_FOLDER_PICKER_ZENITY", "cancelled", 1) == 0;
    passed = passed && Datalab_FolderPicker_Select("Cancel", NULL, output, sizeof(output)) == DATALAB_FOLDER_PICKER_CANCELLED &&
             output[0] == '\0' && access(marker, F_OK) != 0;
    remove_fixture(root, zenity, kdialog, args_log, marker);
    return passed;
}

static bool test_unavailable(void) {
    char template[PATH_MAX];
    char output[PATH_MAX];
    char *root = make_fixture_root(template, sizeof(template), "datalab_folder_picker_empty") ? mkdtemp(template) : NULL;
    bool passed = root && setenv("PATH", root, 1) == 0 &&
                  Datalab_FolderPicker_Select("Unavailable", NULL, output, sizeof(output)) == DATALAB_FOLDER_PICKER_UNAVAILABLE && output[0] == '\0';
    if (root) (void)rmdir(root);
    return passed;
}

int main(void) {
    const bool zenity = test_zenity_folder_arguments();
    const bool kdialog = test_kdialog_fallback();
    const bool cancel = test_cancel_does_not_fallback();
    const bool unavailable = test_unavailable();
    const bool passed = zenity && kdialog && cancel && unavailable;
    if (!zenity) fprintf(stderr, "datalab_folder_picker_test: zenity argument fixture failed\n");
    if (!kdialog) fprintf(stderr, "datalab_folder_picker_test: kdialog fallback fixture failed\n");
    if (!cancel) fprintf(stderr, "datalab_folder_picker_test: cancel fixture failed\n");
    if (!unavailable) fprintf(stderr, "datalab_folder_picker_test: unavailable fixture failed\n");
    if (!passed) fprintf(stderr, "datalab_folder_picker_test: failed\n");
    return passed ? 0 : 1;
}
