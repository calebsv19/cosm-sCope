#ifndef DATALAB_INPUT_FILE_LOADER_H
#define DATALAB_INPUT_FILE_LOADER_H

#include "core_base.h"
#include "data/pack_loader.h"

#define DATALAB_INPUT_IMAGE_MAX_DIMENSION 16384u
#define DATALAB_INPUT_IMAGE_MAX_PIXELS 67108864u
#define DATALAB_INPUT_IMAGE_MAX_BYTES 268435456u

int datalab_input_file_is_pack(const char *path);
int datalab_input_file_is_bmp(const char *path);
int datalab_input_file_is_supported(const char *path);

CoreResult datalab_input_image_bounds(uint32_t width,
                                      uint32_t height,
                                      size_t *out_row_bytes,
                                      size_t *out_image_bytes);
CoreResult datalab_load_input_file(const char *path, DatalabFrame *out_frame);

#endif
