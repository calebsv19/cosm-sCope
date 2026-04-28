#ifndef DATALAB_INPUT_FILE_LOADER_H
#define DATALAB_INPUT_FILE_LOADER_H

#include "core_base.h"
#include "data/pack_loader.h"

int datalab_input_file_is_pack(const char *path);
int datalab_input_file_is_bmp(const char *path);
int datalab_input_file_is_supported(const char *path);

CoreResult datalab_load_input_file(const char *path, DatalabFrame *out_frame);

#endif
