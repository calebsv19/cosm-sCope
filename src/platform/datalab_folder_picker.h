#ifndef DATALAB_FOLDER_PICKER_H
#define DATALAB_FOLDER_PICKER_H

#include <stddef.h>

typedef enum DatalabFolderPickerResult {
    DATALAB_FOLDER_PICKER_SELECTED = 0,
    DATALAB_FOLDER_PICKER_CANCELLED,
    DATALAB_FOLDER_PICKER_UNAVAILABLE,
    DATALAB_FOLDER_PICKER_FAILED
} DatalabFolderPickerResult;

/* Opens a native folder chooser without routing prompt or path text through a shell. */
DatalabFolderPickerResult Datalab_FolderPicker_Select(const char *prompt,
                                                       const char *initial_directory,
                                                       char *out_path,
                                                       size_t out_path_size);

#endif
