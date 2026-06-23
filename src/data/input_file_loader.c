#include "data/input_file_loader.h"

#include <stdint.h>
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

CoreResult datalab_input_image_bounds(uint32_t width,
                                      uint32_t height,
                                      size_t *out_row_bytes,
                                      size_t *out_image_bytes) {
    uint64_t pixels = 0u;
    uint64_t row_bytes = 0u;
    uint64_t image_bytes = 0u;
    if (out_row_bytes) {
        *out_row_bytes = 0u;
    }
    if (out_image_bytes) {
        *out_image_bytes = 0u;
    }
    if (width == 0u || height == 0u) {
        return (CoreResult){ CORE_ERR_FORMAT, "invalid bmp dimensions" };
    }
    if (width > DATALAB_INPUT_IMAGE_MAX_DIMENSION ||
        height > DATALAB_INPUT_IMAGE_MAX_DIMENSION) {
        return (CoreResult){ CORE_ERR_FORMAT, "bmp dimensions exceed safety limit" };
    }
    pixels = (uint64_t)width * (uint64_t)height;
    if (pixels > DATALAB_INPUT_IMAGE_MAX_PIXELS) {
        return (CoreResult){ CORE_ERR_FORMAT, "bmp pixel count exceeds safety limit" };
    }
    row_bytes = (uint64_t)width * 4u;
    image_bytes = pixels * 4u;
    if (row_bytes > (uint64_t)SIZE_MAX || image_bytes > (uint64_t)SIZE_MAX ||
        image_bytes > DATALAB_INPUT_IMAGE_MAX_BYTES) {
        return (CoreResult){ CORE_ERR_FORMAT, "bmp byte size exceeds safety limit" };
    }
    if (out_row_bytes) {
        *out_row_bytes = (size_t)row_bytes;
    }
    if (out_image_bytes) {
        *out_image_bytes = (size_t)image_bytes;
    }
    return core_result_ok();
}

static CoreResult datalab_load_bmp_file(const char *path, DatalabFrame *out_frame) {
    SDL_Surface *loaded = NULL;
    SDL_Surface *rgba = NULL;
    size_t row_bytes = 0u;
    size_t image_bytes = 0u;
    uint8_t *rgba_copy = NULL;
    int image_width = 0;
    int image_height = 0;
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
    image_width = rgba->w;
    image_height = rgba->h;
    {
        CoreResult bounds_r = datalab_input_image_bounds((uint32_t)image_width,
                                                         (uint32_t)image_height,
                                                         &row_bytes,
                                                         &image_bytes);
        if (bounds_r.code != CORE_OK) {
            SDL_FreeSurface(rgba);
            return bounds_r;
        }
    }
    if (!rgba->pixels || rgba->pitch < (int)row_bytes) {
        SDL_FreeSurface(rgba);
        return (CoreResult){ CORE_ERR_FORMAT, "invalid bmp row pitch" };
    }
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
    out_frame->width = (uint32_t)image_width;
    out_frame->height = (uint32_t)image_height;
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
