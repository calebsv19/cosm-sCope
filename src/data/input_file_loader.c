#include "data/input_file_loader.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

#include <SDL2/SDL.h>

#include "core_base.h"

static int datalab_has_extension(const char *path, const char *extension) {
    size_t path_len = 0u;
    size_t ext_len = 0u;
    if (!path || !extension) {
        return 0;
    }
    path_len = strlen(path);
    ext_len = strlen(extension);
    if (path_len < ext_len) {
        return 0;
    }
    return strcasecmp(path + (path_len - ext_len), extension) == 0;
}

static CoreResult datalab_load_bmp_file(const char *path, DatalabFrame *out_frame) {
    SDL_Surface *loaded = NULL;
    SDL_Surface *rgba = NULL;
    size_t row_bytes = 0u;
    size_t image_bytes = 0u;
    uint8_t *rgba_copy = NULL;
    if (!path || !out_frame) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid bmp load request" };
    }
    loaded = SDL_LoadBMP(path);
    if (!loaded) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0u);
    SDL_FreeSurface(loaded);
    loaded = NULL;
    if (!rgba) {
        return (CoreResult){ CORE_ERR_IO, SDL_GetError() };
    }
    if (rgba->w <= 0 || rgba->h <= 0) {
        SDL_FreeSurface(rgba);
        return (CoreResult){ CORE_ERR_FORMAT, "invalid bmp dimensions" };
    }

    row_bytes = (size_t)rgba->w * 4u;
    image_bytes = row_bytes * (size_t)rgba->h;
    rgba_copy = (uint8_t *)core_alloc(image_bytes);
    if (!rgba_copy) {
        SDL_FreeSurface(rgba);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }

    for (int y = 0; y < rgba->h; ++y) {
        const uint8_t *src_row = (const uint8_t *)rgba->pixels + ((size_t)y * (size_t)rgba->pitch);
        uint8_t *dst_row = rgba_copy + ((size_t)y * row_bytes);
        memcpy(dst_row, src_row, row_bytes);
    }
    SDL_FreeSurface(rgba);

    datalab_frame_init(out_frame);
    out_frame->profile = DATALAB_PROFILE_IMAGE;
    out_frame->width = (uint32_t)row_bytes / 4u;
    out_frame->height = (uint32_t)(image_bytes / row_bytes);
    out_frame->logical_width = out_frame->width;
    out_frame->logical_height = out_frame->height;
    out_frame->drawing_rgba = rgba_copy;
    return core_result_ok();
}

int datalab_input_file_is_pack(const char *path) {
    return datalab_has_extension(path, ".pack");
}

int datalab_input_file_is_bmp(const char *path) {
    return datalab_has_extension(path, ".bmp");
}

int datalab_input_file_is_supported(const char *path) {
    return datalab_input_file_is_pack(path) || datalab_input_file_is_bmp(path);
}

CoreResult datalab_load_input_file(const char *path, DatalabFrame *out_frame) {
    if (!path || !out_frame) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid load request" };
    }
    if (datalab_input_file_is_pack(path)) {
        return datalab_load_pack(path, out_frame);
    }
    if (datalab_input_file_is_bmp(path)) {
        return datalab_load_bmp_file(path, out_frame);
    }
    return (CoreResult){ CORE_ERR_NOT_FOUND, "unsupported input file extension (expected .pack or .bmp)" };
}
