#include "app/datalab_w5_acceptance.h"

#include <errno.h>
#include <png.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/datalab_catalog_view.h"
#include "app/datalab_focus_window.h"
#include "app/datalab_input_catalog.h"

static int w5_join(char *out, size_t out_cap, const char *root, const char *leaf) {
    return out && root && leaf && snprintf(out, out_cap, "%s/%s", root, leaf) < (int)out_cap;
}

static int w5_mkdir(const char *path) {
    return path && mkdir(path, 0700) == 0;
}

static int w5_write_bmp(const char *path, int width, int height, Uint32 color) {
    SDL_Surface *surface = NULL;
    if (!path || width <= 0 || height <= 0) return 0;
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return 0;
    SDL_FillRect(surface, NULL, color);
    if (SDL_SaveBMP(surface, path) != 0) {
        SDL_FreeSurface(surface);
        return 0;
    }
    SDL_FreeSurface(surface);
    return 1;
}

static int w5_write_png(const char *path) {
    static const png_byte pixels[] = {
        255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u,
        0u, 0u, 255u, 255u, 255u, 255u, 0u, 255u
    };
    png_bytep rows[2] = {(png_bytep)pixels, (png_bytep)(pixels + 8u)};
    FILE *fp = path ? fopen(path, "wb") : NULL;
    png_structp png = fp ? png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL) : NULL;
    png_infop info = png ? png_create_info_struct(png) : NULL;
    int ok = 0;
    if (!fp || !png || !info || setjmp(png_jmpbuf(png))) goto done;
    png_init_io(png, fp);
    png_set_IHDR(png, info, 2u, 2u, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    png_write_image(png, rows);
    png_write_end(png, info);
    ok = 1;
done:
    if (png) png_destroy_write_struct(&png, &info);
    if (fp) fclose(fp);
    return ok;
}

static int w5_write_text(const char *path, const char *text) {
    FILE *fp = path ? fopen(path, "wb") : NULL;
    int ok = 0;
    if (fp) {
        ok = fputs(text, fp) >= 0 && fclose(fp) == 0;
    }
    return ok;
}

static int w5_fill_supported(const char *root, size_t count) {
    char path[DATALAB_APP_PATH_CAP];
    for (size_t i = 0u; i < count; ++i) {
        if (snprintf(path, sizeof(path), "%s/frame_%04zu.%s", root, i + 1u,
                     (i & 1u) ? "png" : "bmp") >= (int)sizeof(path) ||
            ((i & 1u) ? !w5_write_png(path) : !w5_write_bmp(path, 2, 2, 0xff2050ffu))) return 0;
    }
    return 1;
}

static int w5_check_real_boundaries(const char *root, FILE *manifest) {
    static const size_t counts[] = {0u, 1u, 64u, 65u, 160u, 161u, 256u, 257u};
    DatalabInputCatalog catalog;
    char directory[DATALAB_APP_PATH_CAP];
    int ok = 1;
    datalab_input_catalog_init(&catalog);
    for (size_t i = 0u; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        char leaf[48];
        (void)snprintf(leaf, sizeof(leaf), "cardinality_%zu", counts[i]);
        if (!w5_join(directory, sizeof(directory), root, leaf) || !w5_mkdir(directory) ||
            !w5_fill_supported(directory, counts[i]) ||
            datalab_input_catalog_refresh(&catalog, directory, DATALAB_INPUT_CATALOG_REFRESH_INITIAL).code != CORE_OK ||
            datalab_input_catalog_count(&catalog) != counts[i]) {
            ok = 0;
            break;
        }
        fprintf(manifest, "cardinality_%zu=%zu\n", counts[i], catalog.file_count);
    }
    datalab_input_catalog_destroy(&catalog);
    return ok;
}

static int w5_check_mixed_directory(const char *root, FILE *manifest) {
    DatalabInputCatalog catalog;
    char mixed[DATALAB_APP_PATH_CAP];
    char path[DATALAB_APP_PATH_CAP];
    int ok = 0;
    if (!w5_join(mixed, sizeof(mixed), root, "mixed_300") || !w5_mkdir(mixed)) return 0;
    for (size_t i = 0u; i < 299u; ++i) {
        if (snprintf(path, sizeof(path), "%s/mixed_%04zu.%s", mixed, i + 1u,
                     (i & 1u) ? "png" : "bmp") >= (int)sizeof(path)) return 0;
        if ((i == 1u && !w5_write_bmp(path, 320, 180, 0xff8040ffu)) ||
            (i == 2u && !w5_write_bmp(path, 1024, 768, 0xff40a0ffu)) ||
            (i != 1u && i != 2u &&
             ((i & 1u) ? !w5_write_png(path) : !w5_write_bmp(path, 2, 2, 0xff2050ffu)))) return 0;
    }
    if (!w5_join(path, sizeof(path), mixed, "corrupt_supported.png") || !w5_write_text(path, "not a png\n")) return 0;
    datalab_input_catalog_init(&catalog);
    if (datalab_input_catalog_refresh(&catalog, mixed, DATALAB_INPUT_CATALOG_REFRESH_INITIAL).code == CORE_OK &&
        catalog.file_count == 300u && datalab_input_catalog_file_is_current(&catalog, mixed, "mixed_0002.png") &&
        w5_join(path, sizeof(path), mixed, "mixed_0001.bmp") && unlink(path) == 0 &&
        datalab_input_catalog_refresh(&catalog, mixed, DATALAB_INPUT_CATALOG_REFRESH_EXPLICIT).code == CORE_OK &&
        catalog.file_count == 299u) {
        fprintf(manifest, "mixed_initial=300\nmixed_after_removed=299\nchanged_file=mixed_0002.png\ncorrupt_file=corrupt_supported.png\n");
        ok = 1;
    }
    datalab_input_catalog_destroy(&catalog);
    return ok;
}

static int w5_check_virtual_scale(FILE *manifest) {
    DatalabInputCatalog catalog;
    DatalabCatalogView view;
    char selected[DATALAB_APP_PATH_CAP];
    int ok = 0;
    datalab_input_catalog_init(&catalog);
    datalab_catalog_view_init(&view);
    if (datalab_input_catalog_generate_fixture(&catalog, 1000000u).code == CORE_OK &&
        catalog.metrics.peak_bytes <= 58331648u) {
        datalab_catalog_view_bind(&view, &catalog);
        datalab_catalog_view_set_selected_index(&view, 256u);
        datalab_catalog_view_step_selected(&view, -1000000, 0);
        datalab_catalog_view_step_selected(&view, 1000000, 0);
        datalab_catalog_view_set_filter(&view, "frame_0001000000");
        while (datalab_catalog_view_filter_scanning(&view)) datalab_catalog_view_step_filter(&view, 4096u);
        if (datalab_catalog_view_count(&view) == 1u &&
            datalab_catalog_view_selected_name_copy(&view, selected, sizeof(selected)) &&
            strcmp(selected, "frame_0001000000.png") == 0) {
            fprintf(manifest, "synthetic_catalog=1000000\nsynthetic_peak_bytes=%zu\nvirtual_rows=%u\n",
                    catalog.metrics.peak_bytes, DATALAB_CATALOG_VIEW_PAGE_WINDOW);
            ok = 1;
        }
    }
    datalab_catalog_view_destroy(&view);
    datalab_input_catalog_destroy(&catalog);
    return ok;
}

static int w5_check_focus_and_events(FILE *manifest) {
    DatalabFocusWindow window;
    DatalabFocusWindowIntent intent;
    const DatalabFocusWindowMetrics *metrics = NULL;
    SDL_Event event;
    Uint32 event_type = SDL_RegisterEvents(1);
    unsigned int events_seen = 0u;
    unsigned int event_failures = 0u;
    if (event_type == (Uint32)-1) return 0;
    datalab_focus_window_init(&window);
    for (uint64_t i = 0u; i < 20000u; ++i) {
        datalab_focus_window_select(&window, 7u + (i == 10000u), 1000000u,
                                    (i * 7919u) % 1000000u, (i & 1u) ? 1 : -1,
                                    4u, 1);
        while (datalab_focus_window_pop_intent(&window, &intent))
            datalab_focus_window_note_complete(&window, &intent, 1);
        if (i == 5000u) datalab_focus_window_set_pressure(&window, DATALAB_FOCUS_WINDOW_PRESSURE_CRITICAL);
        if (i == 5001u) datalab_focus_window_set_pressure(&window, DATALAB_FOCUS_WINDOW_PRESSURE_NORMAL);
        memset(&event, 0, sizeof(event));
        event.type = event_type;
        if (SDL_PushEvent(&event) < 0) {
            ++event_failures;
        } else {
            int found = 0;
            while (SDL_PollEvent(&event)) {
                if (event.type == event_type) found = 1;
            }
            if (found) ++events_seen;
            else ++event_failures;
        }
    }
    metrics = datalab_focus_window_metrics(&window);
    if (!metrics) return 0;
    fprintf(manifest, "soak_operations=20000\nevents_seen=%u\nevent_failures=%u\npeak_scheduler_intents=%u\nradius=%u\ncompleted=%llu\nstale=%llu\n",
            events_seen, event_failures, metrics->peak_queued, metrics->radius,
            (unsigned long long)metrics->completed, (unsigned long long)metrics->stale);
    return metrics->peak_queued <= DATALAB_FOCUS_WINDOW_INTENT_CAPACITY &&
           metrics->radius <= DATALAB_FOCUS_WINDOW_MAX_RADIUS && events_seen == 20000u && event_failures == 0u;
}

int datalab_w5_acceptance_run(const char *output_root) {
    char manifest_path[DATALAB_APP_PATH_CAP];
    FILE *manifest = NULL;
    int ok = 0;
    if (!output_root || output_root[0] == '\0' || !w5_mkdir(output_root) ||
        !w5_join(manifest_path, sizeof(manifest_path), output_root, "w5_manifest.txt") ||
        !(manifest = fopen(manifest_path, "wb"))) {
        fprintf(stderr, "datalab W5 acceptance: output directory must be new and writable\n");
        return 2;
    }
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        fclose(manifest);
        return 2;
    }
    fprintf(manifest, "acceptance_driver=W5-explicit\nfixture_root=%s\n", output_root);
    ok = w5_check_real_boundaries(output_root, manifest) &&
         w5_check_mixed_directory(output_root, manifest) &&
         w5_check_virtual_scale(manifest) &&
         w5_check_focus_and_events(manifest);
    fprintf(manifest, "result=%s\n", ok ? "pass" : "fail");
    fclose(manifest);
    SDL_Quit();
    printf("w5_acceptance_result=%s\nartifact_root=%s\n", ok ? "pass" : "fail", output_root);
    return ok ? 0 : 1;
}
