#include "render/render_view_internal.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL2/SDL_ttf.h>

#include "core_font.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

static float g_datalab_text_zoom_multiplier = 1.0f;

typedef struct DatalabFontCacheEntry {
    int point_size;
    TTF_Font *font;
} DatalabFontCacheEntry;

typedef struct DatalabTextRendererState {
    int init_attempted;
    int ttf_ready;
    int ttf_owner;
    int fallback_warned;
    CoreFontRoleSpec ui_regular_role;
    char font_path[PATH_MAX];
    DatalabFontCacheEntry cache[16];
    size_t cache_count;
} DatalabTextRendererState;

static DatalabTextRendererState g_text_renderer;

void datalab_set_text_zoom_step(int step) {
    g_datalab_text_zoom_multiplier = datalab_text_zoom_step_multiplier(step);
}

void datalab_sync_text_zoom(const DatalabAppState *app_state) {
    int step = 0;
    if (app_state) {
        step = app_state->text_zoom_step;
    }
    datalab_set_text_zoom_step(step);
}

int datalab_scaled_px(float px) {
    float scaled = px * g_datalab_text_zoom_multiplier;
    if (scaled < 1.0f) {
        scaled = 1.0f;
    }
    return (int)lroundf(scaled);
}

static int datalab_file_exists(const char *path) {
    struct stat st;
    if (!path || !path[0]) {
        return 0;
    }
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static const char *datalab_basename(const char *path) {
    const char *base = NULL;
    if (!path || !path[0]) {
        return "";
    }
    base = strrchr(path, '/');
    if (!base) {
        base = strrchr(path, '\\');
    }
    return base ? (base + 1) : path;
}

static int datalab_path_is_absolute(const char *path) {
    if (!path || !path[0]) {
        return 0;
    }
#if defined(_WIN32)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return 1;
    }
#endif
    return path[0] == '/';
}

static int datalab_resolve_from_base_search(const char *relative_path,
                                            char *out_path,
                                            size_t out_path_cap) {
    char *base_path = NULL;
    int depth = 0;
    if (!relative_path || !relative_path[0] || !out_path || out_path_cap == 0) {
        return 0;
    }
    if (datalab_path_is_absolute(relative_path)) {
        return 0;
    }
    base_path = SDL_GetBasePath();
    if (!base_path || !base_path[0]) {
        if (base_path) {
            SDL_free(base_path);
        }
        return 0;
    }
    for (depth = 0; depth <= 8; ++depth) {
        char candidate[PATH_MAX];
        int written = 0;
        size_t offset = 0u;
        int i = 0;

        written = snprintf(candidate, sizeof(candidate), "%s", base_path);
        if (written <= 0 || (size_t)written >= sizeof(candidate)) {
            continue;
        }
        offset = (size_t)written;
        for (i = 0; i < depth; ++i) {
            written = snprintf(candidate + offset,
                               sizeof(candidate) - offset,
                               "../");
            if (written <= 0 || (size_t)written >= (sizeof(candidate) - offset)) {
                offset = 0u;
                break;
            }
            offset += (size_t)written;
        }
        if (offset == 0u) {
            continue;
        }
        written = snprintf(candidate + offset,
                           sizeof(candidate) - offset,
                           "%s",
                           relative_path);
        if (written <= 0 || (size_t)written >= (sizeof(candidate) - offset)) {
            continue;
        }
        if (datalab_file_exists(candidate)) {
            snprintf(out_path, out_path_cap, "%s", candidate);
            SDL_free(base_path);
            return 1;
        }
    }
    SDL_free(base_path);
    return 0;
}

static int datalab_resolve_font_path(const char *path, char *out_path, size_t out_path_cap) {
    const char *base = NULL;
    char scratch_out[PATH_MAX];
    char candidates[4][PATH_MAX];
    size_t i = 0u;
    if (!path || !path[0]) {
        return 0;
    }
    if (!out_path || out_path_cap == 0) {
        out_path = scratch_out;
        out_path_cap = sizeof(scratch_out);
    }
    if (datalab_file_exists(path)) {
        snprintf(out_path, out_path_cap, "%s", path);
        return 1;
    }
    if (datalab_resolve_from_base_search(path, out_path, out_path_cap)) {
        return 1;
    }

    base = datalab_basename(path);
    if (!base || !base[0]) {
        return 0;
    }

    snprintf(candidates[0], sizeof(candidates[0]), "third_party/codework_shared/assets/fonts/%s", base);
    snprintf(candidates[1], sizeof(candidates[1]), "shared/assets/fonts/%s", base);
    snprintf(candidates[2], sizeof(candidates[2]), "Resources/shared/assets/fonts/%s", base);
    snprintf(candidates[3], sizeof(candidates[3]), "../Resources/shared/assets/fonts/%s", base);

    for (i = 0u; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
        const char *rel = candidates[i];
        if (datalab_file_exists(rel)) {
            snprintf(out_path, out_path_cap, "%s", rel);
            return 1;
        }
        if (datalab_resolve_from_base_search(rel, out_path, out_path_cap)) {
            return 1;
        }
    }
    return 0;
}

static int datalab_stat_font_path_exists(const char *path, void *user) {
    char *resolved = (char *)user;
    if (!path || !path[0]) {
        return 0;
    }
    if (resolved && datalab_resolve_font_path(path, resolved, PATH_MAX)) {
        return 1;
    }
    return datalab_resolve_font_path(path, NULL, 0);
}

static int datalab_resolve_font_setup(void) {
    const char *preset_name = getenv("DATALAB_FONT_PRESET");
    CoreFontPreset preset = {0};
    CoreResult r;
    const char *chosen_path = NULL;
    char resolved_path[PATH_MAX] = {0};
    if (!preset_name || !preset_name[0]) {
        preset_name = "ide";
    }
    r = core_font_get_preset_by_name(preset_name, &preset);
    if (r.code != CORE_OK) {
        r = core_font_get_preset(CORE_FONT_PRESET_IDE, &preset);
        if (r.code != CORE_OK) {
            return 0;
        }
    }
    r = core_font_resolve_role(&preset, CORE_FONT_ROLE_UI_REGULAR, &g_text_renderer.ui_regular_role);
    if (r.code != CORE_OK) {
        return 0;
    }
    r = core_font_choose_path(&g_text_renderer.ui_regular_role,
                              datalab_stat_font_path_exists,
                              resolved_path,
                              &chosen_path);
    if (r.code != CORE_OK && (!resolved_path[0] || !chosen_path || !chosen_path[0])) {
        if (!datalab_resolve_font_path(g_text_renderer.ui_regular_role.fallback_path,
                                       resolved_path,
                                       sizeof(resolved_path))) {
            return 0;
        }
    }
    if (!resolved_path[0] && chosen_path && chosen_path[0]) {
        snprintf(resolved_path, sizeof(resolved_path), "%s", chosen_path);
    }
    if (!resolved_path[0]) {
        return 0;
    }
    snprintf(g_text_renderer.font_path, sizeof(g_text_renderer.font_path), "%s", resolved_path);
    return 1;
}

int datalab_text_renderer_init(void) {
    if (g_text_renderer.init_attempted) {
        return g_text_renderer.ttf_ready;
    }
    memset(&g_text_renderer, 0, sizeof(g_text_renderer));
    g_text_renderer.init_attempted = 1;
    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            return 0;
        }
        g_text_renderer.ttf_owner = 1;
    }
    if (!datalab_resolve_font_setup()) {
        if (g_text_renderer.ttf_owner && TTF_WasInit()) {
            TTF_Quit();
        }
        g_text_renderer.ttf_owner = 0;
        return 0;
    }
    g_text_renderer.ttf_ready = 1;
    return 1;
}

void datalab_text_renderer_shutdown(void) {
    size_t i = 0u;
    for (i = 0u; i < g_text_renderer.cache_count; ++i) {
        if (g_text_renderer.cache[i].font) {
            TTF_CloseFont(g_text_renderer.cache[i].font);
            g_text_renderer.cache[i].font = NULL;
        }
    }
    g_text_renderer.cache_count = 0u;
    g_text_renderer.ttf_ready = 0;
    if (g_text_renderer.ttf_owner && TTF_WasInit()) {
        TTF_Quit();
    }
    g_text_renderer.ttf_owner = 0;
    g_text_renderer.init_attempted = 0;
}

static CoreFontTextSizeTier datalab_text_tier_for_scale(int scale) {
    if (scale >= 2) {
        return CORE_FONT_TEXT_SIZE_PARAGRAPH;
    }
    return CORE_FONT_TEXT_SIZE_CAPTION;
}

static int datalab_point_size_for_scale(int scale) {
    int point_size = 9;
    int scaled_point = 0;
    if (scale < 1) {
        scale = 1;
    }
    if (core_font_point_size_for_tier(&g_text_renderer.ui_regular_role,
                                      datalab_text_tier_for_scale(scale),
                                      &point_size).code != CORE_OK) {
        point_size = g_text_renderer.ui_regular_role.point_size > 0 ? g_text_renderer.ui_regular_role.point_size : 9;
    }
    if (scale > 2) {
        point_size += (scale - 2) * 2;
    }
    scaled_point = (int)lroundf((float)point_size * g_datalab_text_zoom_multiplier);
    if (scaled_point < 6) {
        scaled_point = 6;
    }
    return scaled_point;
}

static TTF_Font *datalab_get_font_for_scale(int scale) {
    int point_size = 0;
    size_t i = 0u;
    TTF_Font *font = NULL;

    if (!datalab_text_renderer_init()) {
        return NULL;
    }
    point_size = datalab_point_size_for_scale(scale);
    for (i = 0u; i < g_text_renderer.cache_count; ++i) {
        if (g_text_renderer.cache[i].point_size == point_size) {
            return g_text_renderer.cache[i].font;
        }
    }
    font = TTF_OpenFont(g_text_renderer.font_path, point_size);
    if (!font) {
        return NULL;
    }
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    TTF_SetFontKerning(font, 1);

    if (g_text_renderer.cache_count < (sizeof(g_text_renderer.cache) / sizeof(g_text_renderer.cache[0]))) {
        g_text_renderer.cache[g_text_renderer.cache_count].point_size = point_size;
        g_text_renderer.cache[g_text_renderer.cache_count].font = font;
        g_text_renderer.cache_count++;
    }
    return font;
}

static int datalab_draw_text_ttf(SDL_Renderer *renderer,
                                 int x,
                                 int y,
                                 const char *text,
                                 int scale,
                                 uint8_t r,
                                 uint8_t g,
                                 uint8_t b,
                                 uint8_t a) {
    TTF_Font *font = NULL;
    SDL_Color color;
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;
    SDL_Rect dst = {0};

    if (!renderer || !text || text[0] == '\0') {
        return 1;
    }
    font = datalab_get_font_for_scale(scale);
    if (!font) {
        return 0;
    }
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return 0;
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return 0;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    dst.x = x;
    dst.y = y;
    dst.w = surface->w;
    dst.h = surface->h;
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    return 1;
}

static int datalab_measure_text_ttf(const char *text, int scale, int *out_width, int *out_height) {
    TTF_Font *font = NULL;
    int width = 0;
    int height = 0;
    if (!text || !out_width || !out_height) {
        return 0;
    }
    font = datalab_get_font_for_scale(scale);
    if (!font) {
        return 0;
    }
    if (text[0] == '\0') {
        *out_width = 0;
        *out_height = TTF_FontHeight(font);
        if (*out_height <= 0) {
            *out_height = datalab_scaled_px(8.0f);
        }
        return 1;
    }
    if (TTF_SizeUTF8(font, text, &width, &height) != 0) {
        return 0;
    }
    if (height <= 0) {
        height = TTF_FontHeight(font);
    }
    if (height <= 0) {
        height = datalab_scaled_px(8.0f);
    }
    *out_width = width;
    *out_height = height;
    return 1;
}

static const char *glyph5x7(char c) {
    switch (c) {
        case 'A': return "01110""10001""10001""11111""10001""10001""10001";
        case 'D': return "11110""10001""10001""10001""10001""10001""11110";
        case 'B': return "11110""10001""10001""11110""10001""10001""11110";
        case 'C': return "01111""10000""10000""10000""10000""10000""01111";
        case 'E': return "11111""10000""10000""11110""10000""10000""11111";
        case 'F': return "11111""10000""10000""11110""10000""10000""10000";
        case 'G': return "01110""10001""10000""10011""10001""10001""01110";
        case 'H': return "10001""10001""10001""11111""10001""10001""10001";
        case 'I': return "11111""00100""00100""00100""00100""00100""11111";
        case 'K': return "10001""10010""10100""11000""10100""10010""10001";
        case 'L': return "10000""10000""10000""10000""10000""10000""11111";
        case 'M': return "10001""11011""10101""10101""10001""10001""10001";
        case 'N': return "10001""11001""10101""10011""10001""10001""10001";
        case 'O': return "01110""10001""10001""10001""10001""10001""01110";
        case 'P': return "11110""10001""10001""11110""10000""10000""10000";
        case 'R': return "11110""10001""10001""11110""10100""10010""10001";
        case 'S': return "01111""10000""10000""01110""00001""00001""11110";
        case 'T': return "11111""00100""00100""00100""00100""00100""00100";
        case 'U': return "10001""10001""10001""10001""10001""10001""01110";
        case 'V': return "10001""10001""10001""10001""10001""01010""00100";
        case 'W': return "10001""10001""10001""10101""10101""10101""01010";
        case 'X': return "10001""10001""01010""00100""01010""10001""10001";
        case 'Y': return "10001""10001""01010""00100""00100""00100""00100";
        case 'Z': return "11111""00001""00010""00100""01000""10000""11111";
        case '0': return "01110""10001""10011""10101""11001""10001""01110";
        case '1': return "00100""01100""00100""00100""00100""00100""01110";
        case '2': return "01110""10001""00001""00010""00100""01000""11111";
        case '3': return "11110""00001""00001""01110""00001""00001""11110";
        case '4': return "00010""00110""01010""10010""11111""00010""00010";
        case '5': return "11111""10000""11110""00001""00001""10001""01110";
        case '6': return "00110""01000""10000""11110""10001""10001""01110";
        case '7': return "11111""00001""00010""00100""01000""01000""01000";
        case '8': return "01110""10001""10001""01110""10001""10001""01110";
        case '9': return "01110""10001""10001""01111""00001""00010""11100";
        case '.': return "00000""00000""00000""00000""00000""00110""00110";
        case '-': return "00000""00000""00000""11111""00000""00000""00000";
        case '_': return "00000""00000""00000""00000""00000""00000""11111";
        case '+': return "00000""00100""00100""11111""00100""00100""00000";
        case ':': return "00000""00100""00100""00000""00100""00100""00000";
        case '=': return "00000""11111""00000""11111""00000""00000""00000";
        case 'a': return glyph5x7('A');
        case 'b': return glyph5x7('B');
        case 'c': return glyph5x7('C');
        case 'd': return glyph5x7('D');
        case 'e': return glyph5x7('E');
        case 'f': return glyph5x7('F');
        case 'g': return glyph5x7('G');
        case 'h': return glyph5x7('H');
        case 'i': return glyph5x7('I');
        case 'j': return glyph5x7('I');
        case 'k': return glyph5x7('K');
        case 'l': return glyph5x7('L');
        case 'm': return glyph5x7('M');
        case 'n': return glyph5x7('N');
        case 'o': return glyph5x7('O');
        case 'p': return glyph5x7('P');
        case 'q': return glyph5x7('O');
        case 'r': return glyph5x7('R');
        case 's': return glyph5x7('S');
        case 't': return glyph5x7('T');
        case 'u': return glyph5x7('U');
        case 'v': return glyph5x7('V');
        case 'w': return glyph5x7('W');
        case 'x': return glyph5x7('X');
        case 'y': return glyph5x7('Y');
        case 'z': return glyph5x7('Z');
        case ' ': return "00000""00000""00000""00000""00000""00000""00000";
        default: return "00000""00000""00000""00000""00000""00000""00000";
    }
}

static void draw_text_5x7_bitmap(SDL_Renderer *renderer,
                                 int x,
                                 int y,
                                 const char *text,
                                 int scale,
                                 uint8_t r,
                                 uint8_t g,
                                 uint8_t b,
                                 uint8_t a) {
    float draw_scale;
    float glyph_advance;
    if (!renderer || !text || scale < 1) {
        return;
    }
    draw_scale = ((float)scale) * g_datalab_text_zoom_multiplier;
    if (draw_scale < 0.35f) {
        draw_scale = 0.35f;
    }
    glyph_advance = 6.0f * draw_scale;
    if (glyph_advance < 1.0f) {
        glyph_advance = 1.0f;
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    for (int i = 0; text[i] != '\0'; ++i) {
        const char *bits = glyph5x7(text[i]);
        float base_x = (float)x + ((float)i * glyph_advance);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                char bit = bits[row * 5 + col];
                if (bit == '1') {
                    int x0 = (int)floorf(base_x + ((float)col * draw_scale));
                    int x1 = (int)floorf(base_x + ((float)(col + 1) * draw_scale));
                    int y0 = (int)floorf((float)y + ((float)row * draw_scale));
                    int y1 = (int)floorf((float)y + ((float)(row + 1) * draw_scale));
                    if (x1 <= x0) {
                        x1 = x0 + 1;
                    }
                    if (y1 <= y0) {
                        y1 = y0 + 1;
                    }
                    SDL_Rect px = { x0, y0, x1 - x0, y1 - y0 };
                    SDL_RenderFillRect(renderer, &px);
                }
            }
        }
    }
}

static void datalab_measure_text_bitmap(const char *text, int scale, int *out_width, int *out_height) {
    float draw_scale = 0.0f;
    float glyph_advance = 0.0f;
    size_t length = 0u;
    if (!out_width || !out_height) {
        return;
    }
    if (!text) {
        *out_width = 0;
        *out_height = 0;
        return;
    }
    if (scale < 1) {
        scale = 1;
    }
    draw_scale = ((float)scale) * g_datalab_text_zoom_multiplier;
    if (draw_scale < 0.35f) {
        draw_scale = 0.35f;
    }
    glyph_advance = 6.0f * draw_scale;
    if (glyph_advance < 1.0f) {
        glyph_advance = 1.0f;
    }
    length = strlen(text);
    *out_width = (int)lroundf((float)length * glyph_advance);
    *out_height = (int)lroundf(7.0f * draw_scale);
    if (*out_height < 1) {
        *out_height = 1;
    }
}

int datalab_measure_text(int scale, const char *text, int *out_width, int *out_height) {
    if (!out_width || !out_height) {
        return 0;
    }
    if (datalab_measure_text_ttf(text ? text : "", scale, out_width, out_height)) {
        return 1;
    }
    datalab_measure_text_bitmap(text ? text : "", scale, out_width, out_height);
    return 0;
}

int datalab_text_line_height(int scale) {
    int width = 0;
    int height = 0;
    (void)datalab_measure_text(scale, "Ag", &width, &height);
    if (height < 1) {
        height = datalab_scaled_px(9.0f);
    }
    return height;
}

void draw_text_5x7(SDL_Renderer *renderer,
                   int x,
                   int y,
                   const char *text,
                   int scale,
                   uint8_t r,
                   uint8_t g,
                   uint8_t b,
                   uint8_t a) {
    if (datalab_draw_text_ttf(renderer, x, y, text, scale, r, g, b, a)) {
        return;
    }
    if (!g_text_renderer.fallback_warned) {
        fprintf(stderr, "[datalab] shared font unavailable, falling back to bitmap glyph text.\n");
        g_text_renderer.fallback_warned = 1;
    }
    draw_text_5x7_bitmap(renderer, x, y, text, scale, r, g, b, a);
}

void draw_text_5x7_clipped(SDL_Renderer *renderer,
                           const SDL_Rect *clip_rect,
                           int x,
                           int y,
                           const char *text,
                           int scale,
                           uint8_t r,
                           uint8_t g,
                           uint8_t b,
                           uint8_t a) {
    SDL_Rect prior_clip = {0};
    SDL_bool had_clip = SDL_FALSE;
    if (!renderer || !text || !clip_rect) {
        draw_text_5x7(renderer, x, y, text, scale, r, g, b, a);
        return;
    }
    had_clip = SDL_RenderIsClipEnabled(renderer);
    if (had_clip == SDL_TRUE) {
        SDL_RenderGetClipRect(renderer, &prior_clip);
    }
    SDL_RenderSetClipRect(renderer, clip_rect);
    draw_text_5x7(renderer, x, y, text, scale, r, g, b, a);
    if (had_clip == SDL_TRUE) {
        SDL_RenderSetClipRect(renderer, &prior_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
}
