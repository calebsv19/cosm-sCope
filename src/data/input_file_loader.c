#include "data/input_file_loader.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

#include <png.h>

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
    out_frame->image_metadata.format = DATALAB_IMAGE_FORMAT_BMP;
    out_frame->image_metadata.transfer = DATALAB_IMAGE_TRANSFER_UNTAGGED_SRGB_ASSUMED;
    out_frame->image_metadata.source_bit_depth = 8u;
    out_frame->image_metadata.source_has_alpha = 0u;
    return core_result_ok();
}

static CoreResult datalab_load_png_file(const char *path, DatalabFrame *out_frame) {
    FILE *fp = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep *rows = NULL;
    uint8_t *rgba = NULL;
    png_uint_32 width = 0u;
    png_uint_32 height = 0u;
    int bit_depth = 0;
    int color_type = 0;
    int intent = 0;
    int has_srgb = 0;
    int has_gamma = 0;
    int has_icc = 0;
    double gamma = 0.0;
    png_charp icc_name = NULL;
    int icc_compression = 0;
    png_bytep icc_profile = NULL;
    png_uint_32 icc_profile_len = 0u;
    size_t row_bytes = 0u;
    size_t image_bytes = 0u;
    CoreResult result = core_result_ok();

    if (!path || !out_frame) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid png load request" };
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return (CoreResult){ CORE_ERR_IO, "failed to open png" };
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "failed to create png reader" };
        goto cleanup;
    }
    info = png_create_info_struct(png);
    if (!info) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "failed to create png info" };
        goto cleanup;
    }
    if (setjmp(png_jmpbuf(png))) {
        result = (CoreResult){ CORE_ERR_FORMAT, "invalid or unsupported png" };
        goto cleanup;
    }
    png_init_io(png, fp);
    png_read_info(png, info);
    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    bit_depth = png_get_bit_depth(png, info);
    color_type = png_get_color_type(png, info);
    has_srgb = png_get_sRGB(png, info, &intent);
    has_gamma = png_get_gAMA(png, info, &gamma);
    has_icc = png_get_iCCP(png, info, &icc_name, &icc_compression, &icc_profile, &icc_profile_len);
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) png_set_filler(png, 0xff, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    result = datalab_input_image_bounds((uint32_t)width, (uint32_t)height, &row_bytes, &image_bytes);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    rgba = (uint8_t *)core_alloc(image_bytes);
    rows = (png_bytep *)core_alloc(sizeof(*rows) * (size_t)height);
    if (!rgba || !rows) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }
    for (png_uint_32 y = 0u; y < height; ++y) {
        rows[y] = rgba + ((size_t)y * row_bytes);
    }
    png_read_image(png, rows);
    png_read_end(png, info);
    datalab_frame_init(out_frame);
    out_frame->profile = DATALAB_PROFILE_IMAGE;
    out_frame->width = (uint32_t)width;
    out_frame->height = (uint32_t)height;
    out_frame->logical_width = out_frame->width;
    out_frame->logical_height = out_frame->height;
    out_frame->drawing_rgba = rgba;
    out_frame->image_metadata.format = DATALAB_IMAGE_FORMAT_PNG;
    out_frame->image_metadata.source_bit_depth = (uint8_t)bit_depth;
    out_frame->image_metadata.source_has_alpha = (uint8_t)(color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
                                                             color_type == PNG_COLOR_TYPE_RGBA ||
                                                             png_get_valid(png, info, PNG_INFO_tRNS));
    out_frame->image_metadata.png_srgb_present = (uint8_t)(has_srgb != 0);
    out_frame->image_metadata.png_gamma_present = (uint8_t)(has_gamma != 0);
    out_frame->image_metadata.png_icc_present = (uint8_t)(has_icc != 0);
    out_frame->image_metadata.png_gamma = gamma;
    if (out_frame->image_metadata.png_icc_present) {
        out_frame->image_metadata.transfer = DATALAB_IMAGE_TRANSFER_ICC_UNTRANSFORMED;
    } else if (out_frame->image_metadata.png_srgb_present) {
        out_frame->image_metadata.transfer = DATALAB_IMAGE_TRANSFER_SRGB;
    } else if (out_frame->image_metadata.png_gamma_present) {
        out_frame->image_metadata.transfer = DATALAB_IMAGE_TRANSFER_GAMA;
    } else {
        out_frame->image_metadata.transfer = DATALAB_IMAGE_TRANSFER_UNTAGGED_SRGB_ASSUMED;
    }
    rgba = NULL;
cleanup:
    core_free(rows);
    core_free(rgba);
    if (png || info) png_destroy_read_struct(&png, &info, NULL);
    if (fp) fclose(fp);
    return result;
}

int datalab_input_file_is_pack(const char *path) {
    return datalab_has_extension(path, ".pack");
}

int datalab_input_file_is_bmp(const char *path) {
    return datalab_has_extension(path, ".bmp");
}

int datalab_input_file_is_png(const char *path) {
    return datalab_has_extension(path, ".png");
}

int datalab_input_file_is_supported(const char *path) {
    return datalab_input_file_is_pack(path) || datalab_input_file_is_bmp(path) ||
           datalab_input_file_is_png(path);
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
    if (datalab_input_file_is_png(path)) {
        return datalab_load_png_file(path, out_frame);
    }
    return (CoreResult){ CORE_ERR_NOT_FOUND, "unsupported input file extension (expected .pack, .bmp, or .png)" };
}
