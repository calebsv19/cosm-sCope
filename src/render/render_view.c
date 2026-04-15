#include "render/render_view.h"
#include "render/render_view_internal.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL2/SDL_ttf.h>

#include "core_font.h"
#include "ui/input.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

static float g_datalab_text_zoom_multiplier = 1.0f;

static int datalab_env_flag_enabled(const char *name) {
    const char *value = NULL;
    if (!name) {
        return 0;
    }
    value = getenv(name);
    if (!value || !value[0]) {
        return 0;
    }
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

int datalab_ir1_diag_enabled(void) {
    return datalab_env_flag_enabled("DATALAB_IR1_DIAG");
}

int datalab_rs1_diag_enabled(void) {
    return datalab_env_flag_enabled("DATALAB_RS1_DIAG");
}

void datalab_set_text_zoom_step(int step) { g_datalab_text_zoom_multiplier = datalab_text_zoom_step_multiplier(step); }

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

static const char *daw_view_mode_name(DatalabViewMode mode) {
    switch (mode) {
        case DATALAB_VIEW_DENSITY: return "markers";
        case DATALAB_VIEW_SPEED: return "waveform";
        case DATALAB_VIEW_DENSITY_VECTOR: return "waveform+markers";
        default: return "unknown";
    }
}

int lane_name_eq(const char *a, const char *b) { return strncmp(a, b, 31) == 0; }

void make_title(const DatalabFrame *frame, const DatalabAppState *state, char *title, size_t title_cap) {
    if (!frame || !state || !title || title_cap == 0) return;

    if (frame->profile == DATALAB_PROFILE_DAW) {
        snprintf(title,
                 title_cap,
                 "DataLab | DAW points=%llu markers=%zu sr=%u mode=%s text=%+d auth=%s",
                 (unsigned long long)frame->point_count,
                 frame->marker_count,
                 frame->sample_rate,
                 daw_view_mode_name(state->view_mode),
                 state->text_zoom_step,
                 state->workspace_authoring_stub_active ? "on" : "off");
        return;
    }

    if (frame->profile == DATALAB_PROFILE_TRACE) {
        size_t cursor = state->trace_cursor_index;
        if (frame->trace_sample_count == 0) cursor = 0;
        snprintf(title,
                 title_cap,
                 "DataLab | TRACE samples=%zu markers=%zu cursor=%zu zoom_stub=%.2f stats_stub=%s text=%+d auth=%s",
                 frame->trace_sample_count,
                 frame->trace_marker_count,
                 cursor,
                 state->trace_zoom_stub,
                 state->trace_selection_stub_active ? "on" : "off",
                 state->text_zoom_step,
                 state->workspace_authoring_stub_active ? "on" : "off");
        return;
    }

    snprintf(title,
             title_cap,
             "DataLab | frame=%llu grid=%ux%u t=%.3f dt=%.3f mode=%s stride=%u text=%+d auth=%s",
             (unsigned long long)frame->frame_index,
             frame->width,
             frame->height,
             frame->time_seconds,
             frame->dt_seconds,
             datalab_view_mode_name(state->view_mode),
             state->vector_stride,
             state->text_zoom_step,
             state->workspace_authoring_stub_active ? "on" : "off");
}

void calc_fit_rect(int ww, int wh, uint32_t fw, uint32_t fh, SDL_Rect *out_rect) {
    if (!out_rect || fw == 0 || fh == 0 || ww <= 0 || wh <= 0) return;

    const float sx = (float)ww / (float)fw;
    const float sy = (float)wh / (float)fh;
    const float scale = sx < sy ? sx : sy;

    const int rw = (int)((float)fw * scale);
    const int rh = (int)((float)fh * scale);
    out_rect->x = (ww - rw) / 2;
    out_rect->y = (wh - rh) / 2;
    out_rect->w = rw;
    out_rect->h = rh;
}

float clamp_unit(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

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
        r = core_font_get_preset(CORE_FONT_PRESET_DAW_DEFAULT, &preset);
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

static int datalab_text_renderer_init(void) {
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

static void datalab_text_renderer_shutdown(void) {
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

static void draw_daw_legend(SDL_Renderer *renderer, const DatalabAppState *app_state, const SDL_Rect *plot_rect) {
    static const char *kTitle = "MODE";
    static const char *kRow1 = "1 WAVEFORM";
    static const char *kRow2 = "2 WAVE+MARKERS";
    static const char *kRow3 = "3 MARKERS";
    int title_w = 0;
    int title_h = 0;
    int row1_w = 0;
    int row1_h = 0;
    int row2_w = 0;
    int row2_h = 0;
    int row3_w = 0;
    int row3_h = 0;
    int inner_pad = 0;
    int row_gap = 0;
    int max_row_w = 0;
    int panel_w = 0;
    int panel_h = 0;
    int cursor_y = 0;
    SDL_Rect panel;
    if (!renderer || !app_state || !plot_rect) {
        return;
    }
    (void)datalab_measure_text(2, kTitle, &title_w, &title_h);
    (void)datalab_measure_text(2, kRow1, &row1_w, &row1_h);
    (void)datalab_measure_text(1, kRow2, &row2_w, &row2_h);
    (void)datalab_measure_text(1, kRow3, &row3_w, &row3_h);

    inner_pad = datalab_scaled_px(8.0f);
    row_gap = datalab_scaled_px(4.0f);
    max_row_w = row1_w;
    if (row2_w > max_row_w) {
        max_row_w = row2_w;
    }
    if (row3_w > max_row_w) {
        max_row_w = row3_w;
    }

    panel_w = max_row_w + (inner_pad * 2);
    if (title_w + (inner_pad * 2) > panel_w) {
        panel_w = title_w + (inner_pad * 2);
    }
    if (panel_w < datalab_scaled_px(180.0f)) {
        panel_w = datalab_scaled_px(180.0f);
    }
    if (panel_w > (plot_rect->w / 2)) {
        panel_w = plot_rect->w / 2;
    }

    panel_h = inner_pad + title_h + row_gap + row1_h + row_gap + row2_h + row_gap + row3_h + inner_pad;
    if (panel_h < datalab_scaled_px(66.0f)) {
        panel_h = datalab_scaled_px(66.0f);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    panel = (SDL_Rect){
        plot_rect->x + datalab_scaled_px(8.0f),
        plot_rect->y + datalab_scaled_px(8.0f),
        panel_w,
        panel_h
    };
    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 180);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 210);
    SDL_RenderDrawRect(renderer, &panel);

    draw_text_5x7(renderer,
                  panel.x + inner_pad,
                  panel.y + inner_pad,
                  kTitle,
                  2,
                  215,
                  220,
                  235,
                  255);

    const uint8_t active_r = 180, active_g = 240, active_b = 220;
    const uint8_t idle_r = 150, idle_g = 155, idle_b = 165;

    const int m = (int)app_state->view_mode;
    cursor_y = panel.y + inner_pad + title_h + row_gap;
    draw_text_5x7(renderer, panel.x + inner_pad, cursor_y, kRow1, 2,
                  (m == DATALAB_VIEW_SPEED) ? active_r : idle_r,
                  (m == DATALAB_VIEW_SPEED) ? active_g : idle_g,
                  (m == DATALAB_VIEW_SPEED) ? active_b : idle_b,
                  255);
    cursor_y += row1_h + row_gap;
    draw_text_5x7(renderer, panel.x + inner_pad, cursor_y, kRow2, 1,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_r : idle_r,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_g : idle_g,
                  (m == DATALAB_VIEW_DENSITY_VECTOR) ? active_b : idle_b,
                  255);
    cursor_y += row2_h + row_gap;
    draw_text_5x7(renderer, panel.x + inner_pad, cursor_y, kRow3, 1,
                  (m == DATALAB_VIEW_DENSITY) ? active_r : idle_r,
                  (m == DATALAB_VIEW_DENSITY) ? active_g : idle_g,
                  (m == DATALAB_VIEW_DENSITY) ? active_b : idle_b,
                  255);
}

static void draw_daw_markers(SDL_Renderer *renderer, const DatalabFrame *frame, const SDL_Rect *plot_rect) {
    if (!renderer || !frame || !plot_rect || frame->marker_count == 0 || plot_rect->w <= 1) {
        return;
    }

    uint64_t start = frame->start_frame;
    uint64_t end = frame->end_frame;
    uint64_t span = end > start ? (end - start) : 1;

    for (size_t i = 0; i < frame->marker_count; ++i) {
        const DatalabDawMarker *m = &frame->markers[i];
        if (m->frame < start || m->frame > end) {
            continue;
        }

        double ratio = (double)(m->frame - start) / (double)span;
        int x = plot_rect->x + (int)(ratio * (double)(plot_rect->w - 1));

        switch (m->kind) {
            case 1: SDL_SetRenderDrawColor(renderer, 255, 180, 70, 220); break;   // tempo
            case 2: SDL_SetRenderDrawColor(renderer, 120, 210, 255, 220); break;  // time signature
            case 3: SDL_SetRenderDrawColor(renderer, 110, 255, 110, 230); break;  // loop start
            case 4: SDL_SetRenderDrawColor(renderer, 255, 110, 110, 230); break;  // loop end
            case 5: SDL_SetRenderDrawColor(renderer, 230, 230, 230, 210); break;  // playhead
            default: SDL_SetRenderDrawColor(renderer, 190, 190, 190, 180); break;
        }

        SDL_RenderDrawLine(renderer, x, plot_rect->y, x, plot_rect->y + plot_rect->h - 1);
    }
}

void render_daw_frame(SDL_Renderer *renderer, const DatalabFrame *frame, const DatalabAppState *app_state) {
    if (!renderer || !frame || !app_state || !frame->wave_min || !frame->wave_max || frame->point_count == 0) {
        return;
    }
    datalab_sync_text_zoom(app_state);

    int ww = 0;
    int wh = 0;
    SDL_GetRendererOutputSize(renderer, &ww, &wh);

    SDL_Rect plot = {
        datalab_scaled_px(30.0f),
        datalab_scaled_px(40.0f),
        ww - datalab_scaled_px(60.0f),
        wh - datalab_scaled_px(80.0f)
    };
    if (plot.w < 10 || plot.h < 10) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 12, 13, 18, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 28, 31, 40, 255);
    SDL_RenderFillRect(renderer, &plot);

    const int center_y = plot.y + (plot.h / 2);
    const float amp = (float)plot.h * 0.46f;

    SDL_SetRenderDrawColor(renderer, 60, 65, 80, 255);
    SDL_RenderDrawLine(renderer, plot.x, center_y, plot.x + plot.w - 1, center_y);

    if (app_state->view_mode != DATALAB_VIEW_DENSITY) {
        SDL_SetRenderDrawColor(renderer, 180, 240, 220, 255);
        for (int x = 0; x < plot.w; ++x) {
            uint64_t idx = (uint64_t)((double)x * (double)(frame->point_count - 1) / (double)(plot.w - 1));
            float min_v = clamp_unit(frame->wave_min[idx]);
            float max_v = clamp_unit(frame->wave_max[idx]);

            int y_top = center_y - (int)(max_v * amp);
            int y_bottom = center_y - (int)(min_v * amp);
            int px = plot.x + x;
            SDL_RenderDrawLine(renderer, px, y_top, px, y_bottom);
        }
    }

    if (app_state->view_mode != DATALAB_VIEW_SPEED) {
        draw_daw_markers(renderer, frame, &plot);
    }

    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 255);
    SDL_RenderDrawRect(renderer, &plot);
    draw_daw_legend(renderer, app_state, &plot);
}


CoreResult datalab_render_run(const DatalabFrame *frame, DatalabAppState *app_state) {
    if (!frame || !app_state) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (frame->profile == DATALAB_PROFILE_PHYSICS) {
        if (!frame->density || !frame->velx || !frame->vely || frame->width == 0 || frame->height == 0) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid physics frame" };
            return r;
        }
    } else if (frame->profile == DATALAB_PROFILE_DAW) {
        if (!frame->wave_min || !frame->wave_max || frame->point_count == 0) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid daw frame" };
            return r;
        }
    } else if (frame->profile == DATALAB_PROFILE_TRACE) {
        if (!frame->trace_samples || frame->trace_sample_count == 0) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid trace frame" };
            return r;
        }
    } else {
        CoreResult r = { CORE_ERR_INVALID_ARG, "unknown frame profile" };
        return r;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }

    SDL_Window *window = SDL_CreateWindow("DataLab",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          1200,
                                          900,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window,
                                                -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        CoreResult r = { CORE_ERR_IO, SDL_GetError() };
        return r;
    }
    (void)datalab_text_renderer_init();

    CoreResult run_r;
    if (frame->profile == DATALAB_PROFILE_DAW) {
        run_r = render_daw_loop(window, renderer, frame, app_state);
    } else if (frame->profile == DATALAB_PROFILE_TRACE) {
        run_r = render_trace_loop(window, renderer, frame, app_state);
    } else {
        run_r = render_physics_loop(window, renderer, frame, app_state);
    }

    datalab_text_renderer_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return run_r;
}
