#include "platform/datalab_folder_picker.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__) && !defined(DATALAB_FOLDER_PICKER_FORCE_LINUX)
#define DATALAB_FOLDER_PICKER_MACOS 1
#else
#define DATALAB_FOLDER_PICKER_MACOS 0
#endif

static void datalab_picker_trim_newline(char *text) {
    size_t length = 0u;
    if (!text) return;
    length = strlen(text);
    while (length > 0u && (text[length - 1u] == '\n' || text[length - 1u] == '\r')) {
        text[--length] = '\0';
    }
}

static DatalabFolderPickerResult datalab_picker_run(const char *const argv[],
                                                    char *out_path,
                                                    size_t out_path_size) {
    int pipe_fds[2] = {-1, -1};
    pid_t child = 0;
    ssize_t bytes_read = 0;
    size_t used = 0u;
    int wait_status = 0;

    if (pipe(pipe_fds) != 0) return DATALAB_FOLDER_PICKER_FAILED;
    child = fork();
    if (child < 0) {
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return DATALAB_FOLDER_PICKER_FAILED;
    }
    if (child == 0) {
        (void)close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _exit(126);
        (void)close(pipe_fds[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(errno == ENOENT ? 127 : 126);
    }

    (void)close(pipe_fds[1]);
    while (used + 1u < out_path_size &&
           (bytes_read = read(pipe_fds[0], out_path + used, out_path_size - used - 1u)) > 0) {
        used += (size_t)bytes_read;
    }
    (void)close(pipe_fds[0]);
    out_path[used] = '\0';
    if (waitpid(child, &wait_status, 0) < 0 || !WIFEXITED(wait_status)) {
        return DATALAB_FOLDER_PICKER_FAILED;
    }
    if (WEXITSTATUS(wait_status) == 127) return DATALAB_FOLDER_PICKER_UNAVAILABLE;
    if (WEXITSTATUS(wait_status) == 1) return DATALAB_FOLDER_PICKER_CANCELLED;
    if (WEXITSTATUS(wait_status) != 0) return DATALAB_FOLDER_PICKER_FAILED;
    datalab_picker_trim_newline(out_path);
    return out_path[0] ? DATALAB_FOLDER_PICKER_SELECTED : DATALAB_FOLDER_PICKER_CANCELLED;
}

#if DATALAB_FOLDER_PICKER_MACOS
static int datalab_picker_escape_applescript_literal(const char *input, char *output, size_t output_size) {
    size_t out_index = 0u;
    if (!input || !output || output_size == 0u) return 0;
    for (size_t index = 0u; input[index] != '\0'; ++index) {
        const char character = input[index];
        if ((character == '\\' || character == '"') && out_index + 2u >= output_size) return 0;
        if (character != '\\' && character != '"' && out_index + 1u >= output_size) return 0;
        if (character == '\\' || character == '"') output[out_index++] = '\\';
        output[out_index++] = character;
    }
    output[out_index] = '\0';
    return 1;
}

static DatalabFolderPickerResult datalab_picker_select_macos(const char *prompt,
                                                              const char *initial_directory,
                                                              char *out_path,
                                                              size_t out_path_size) {
    char escaped_prompt[512];
    char escaped_path[2048];
    char script[3072];
    const char *argv[4];
    if (!datalab_picker_escape_applescript_literal(prompt, escaped_prompt, sizeof(escaped_prompt))) {
        return DATALAB_FOLDER_PICKER_FAILED;
    }
    if (initial_directory && initial_directory[0]) {
        if (!datalab_picker_escape_applescript_literal(initial_directory, escaped_path, sizeof(escaped_path))) {
            return DATALAB_FOLDER_PICKER_FAILED;
        }
        (void)snprintf(script, sizeof(script),
                       "POSIX path of (choose folder with prompt \"%s\" default location POSIX file \"%s\")",
                       escaped_prompt, escaped_path);
    } else {
        (void)snprintf(script, sizeof(script), "POSIX path of (choose folder with prompt \"%s\")", escaped_prompt);
    }
    argv[0] = "/usr/bin/osascript";
    argv[1] = "-e";
    argv[2] = script;
    argv[3] = NULL;
    return datalab_picker_run(argv, out_path, out_path_size);
}
#else
static DatalabFolderPickerResult datalab_picker_select_linux(const char *prompt,
                                                              const char *initial_directory,
                                                              char *out_path,
                                                              size_t out_path_size) {
    const char *zenity_argv[8] = {"zenity", "--file-selection", "--directory", "--title", prompt, NULL, NULL, NULL};
    const char *kdialog_argv[6] = {"kdialog", "--getexistingdirectory", NULL, "--title", prompt, NULL};
    DatalabFolderPickerResult result;
    if (initial_directory && initial_directory[0]) {
        zenity_argv[5] = "--filename";
        zenity_argv[6] = initial_directory;
        kdialog_argv[2] = initial_directory;
    } else {
        kdialog_argv[2] = ".";
    }
    result = datalab_picker_run(zenity_argv, out_path, out_path_size);
    if (result != DATALAB_FOLDER_PICKER_UNAVAILABLE) return result;
    return datalab_picker_run(kdialog_argv, out_path, out_path_size);
}
#endif

DatalabFolderPickerResult Datalab_FolderPicker_Select(const char *prompt,
                                                       const char *initial_directory,
                                                       char *out_path,
                                                       size_t out_path_size) {
    if (!prompt || !prompt[0] || !out_path || out_path_size < 2u) {
        return DATALAB_FOLDER_PICKER_FAILED;
    }
    out_path[0] = '\0';
#if DATALAB_FOLDER_PICKER_MACOS
    return datalab_picker_select_macos(prompt, initial_directory, out_path, out_path_size);
#elif defined(__linux__) || defined(DATALAB_FOLDER_PICKER_FORCE_LINUX)
    return datalab_picker_select_linux(prompt, initial_directory, out_path, out_path_size);
#else
    (void)initial_directory;
    return DATALAB_FOLDER_PICKER_UNAVAILABLE;
#endif
}
