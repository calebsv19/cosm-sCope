#include "render/datalab_vulkan_rollout.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "render/datalab_renderer_backend.h"

static const char *datalab_rollout_path(const char *name, const char *fallback) {
    const char *value = getenv(name);
    return value && value[0] ? value : fallback;
}

static double datalab_rollout_minimum_scale(void) {
    const char *value = getenv("DATALAB_VULKAN_ROLLOUT_MIN_SCALE");
    char *end = NULL;
    double scale = value && value[0] ? strtod(value, &end) : 1.0;
    if (!isfinite(scale) || scale < 1.0 || scale > 4.0 || !end || *end != '\0') {
        return 1.0;
    }
    return scale;
}

static int datalab_rollout_draw(SDL_Renderer *renderer, const char *capture_path) {
    int width = 0;
    int height = 0;
    SDL_Rect header;
    SDL_Rect list;
    SDL_Rect graph;
    SDL_Rect inspector;

    if (!renderer ||
        datalab_renderer_backend_output_size(renderer, &width, &height) != 0 ||
        width < 1 || height < 1) {
        return 0;
    }
    SDL_SetRenderDrawColor(renderer, 10u, 13u, 20u, 255u);
    if (SDL_RenderClear(renderer) != 0) {
        return 0;
    }
    header = (SDL_Rect){width / 24, height / 24, width * 11 / 12, height / 10};
    list = (SDL_Rect){width / 24, height / 6, width / 4, height * 3 / 4};
    graph = (SDL_Rect){width / 3, height / 6, width * 5 / 8, height / 2};
    inspector = (SDL_Rect){width / 3, height * 3 / 4, width * 5 / 8, height / 6};
    SDL_SetRenderDrawColor(renderer, 54u, 36u, 74u, 255u);
    SDL_RenderFillRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 31u, 69u, 61u, 255u);
    SDL_RenderFillRect(renderer, &list);
    SDL_SetRenderDrawColor(renderer, 49u, 74u, 112u, 255u);
    SDL_RenderFillRect(renderer, &graph);
    SDL_SetRenderDrawColor(renderer, 91u, 62u, 38u, 255u);
    SDL_RenderFillRect(renderer, &inspector);
    SDL_SetRenderDrawColor(renderer, 116u, 224u, 255u, 255u);
    SDL_RenderDrawLine(renderer,
                       graph.x,
                       graph.y + graph.h * 3 / 4,
                       graph.x + graph.w / 3,
                       graph.y + graph.h / 3);
    SDL_RenderDrawLine(renderer,
                       graph.x + graph.w / 3,
                       graph.y + graph.h / 3,
                       graph.x + graph.w,
                       graph.y + graph.h / 2);
    if (capture_path &&
        !datalab_renderer_backend_request_capture(renderer, capture_path)) {
        return 0;
    }
    return datalab_renderer_backend_present(renderer);
}

static int datalab_rollout_metrics(SDL_Renderer *renderer,
                                   double minimum_scale,
                                   const char *stage) {
    int logical_width = 0;
    int logical_height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    double scale = 0.0;
    if (!datalab_renderer_backend_drawable_metrics(renderer,
                                                   &logical_width,
                                                   &logical_height,
                                                   &drawable_width,
                                                   &drawable_height,
                                                   &scale) ||
        scale + 0.01 < minimum_scale) {
        return 0;
    }
    fprintf(stdout,
            "DATALAB_VULKAN_DRAWABLE schema=1 stage=%s status=pass logical=%dx%d drawable=%dx%d scale=%.2f\n",
            stage,
            logical_width,
            logical_height,
            drawable_width,
            drawable_height,
            scale);
    return 1;
}

static int datalab_rollout_open(SDL_Window **out_window, SDL_Renderer **out_renderer) {
    SDL_Window *window;
    SDL_Renderer *renderer;
    window = SDL_CreateWindow("DataLab Vulkan Rollout Proof",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              600,
                              420,
                              (int)datalab_renderer_backend_window_flags());
    if (!window) {
        return 0;
    }
    renderer = datalab_renderer_backend_create(window);
    if (!renderer) {
        SDL_DestroyWindow(window);
        return 0;
    }
    *out_window = window;
    *out_renderer = renderer;
    return 1;
}

static void datalab_rollout_close(SDL_Window **window, SDL_Renderer **renderer) {
    if (*renderer) {
        datalab_renderer_backend_destroy(*renderer);
        *renderer = NULL;
    }
    if (*window) {
        SDL_DestroyWindow(*window);
        *window = NULL;
    }
}

int datalab_vulkan_rollout_self_test(void) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    const char *initial_capture = datalab_rollout_path(
        "DATALAB_VULKAN_ROLLOUT_INITIAL_CAPTURE", "datalab-vulkan-initial.bmp");
    const char *resized_capture = datalab_rollout_path(
        "DATALAB_VULKAN_ROLLOUT_RESIZED_CAPTURE", "datalab-vulkan-resized.bmp");
    const double minimum_scale = datalab_rollout_minimum_scale();
    int result = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    if (!datalab_rollout_open(&window, &renderer) ||
        datalab_renderer_backend_kind(renderer) != DATALAB_RENDERER_BACKEND_VULKAN ||
        !datalab_renderer_backend_verify(renderer, "startup", 1) ||
        !datalab_rollout_metrics(renderer, minimum_scale, "startup") ||
        !datalab_rollout_draw(renderer, initial_capture)) {
        goto cleanup;
    }
    SDL_SetWindowSize(window, 760, 520);
    SDL_PumpEvents();
    if (!datalab_rollout_draw(renderer, resized_capture) ||
        !datalab_renderer_backend_verify(renderer, "resized", 1) ||
        !datalab_rollout_metrics(renderer, minimum_scale, "resized")) {
        goto cleanup;
    }
    datalab_rollout_close(&window, &renderer);
    if (!datalab_rollout_open(&window, &renderer) ||
        !datalab_renderer_backend_verify(renderer, "restart", 1) ||
        !datalab_rollout_draw(renderer, NULL)) {
        goto cleanup;
    }
    fprintf(stdout,
            "DATALAB_VULKAN_ROLLOUT schema=1 status=pass compatibility_canvas=sdl picker=backend session=backend resize=recreated capture=native restart=pass\n");
    result = 0;

cleanup:
    datalab_rollout_close(&window, &renderer);
    SDL_Quit();
    return result;
}
