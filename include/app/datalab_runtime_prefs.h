#ifndef DATALAB_RUNTIME_PREFS_H
#define DATALAB_RUNTIME_PREFS_H

#include <stddef.h>

int datalab_runtime_prefs_load_text_zoom_step(int *out_step);
int datalab_runtime_prefs_load_input_root(char *out_path, size_t out_cap);
void datalab_runtime_prefs_save_text_zoom_step(int step);
void datalab_runtime_prefs_save_input_root(const char *path);

#endif
