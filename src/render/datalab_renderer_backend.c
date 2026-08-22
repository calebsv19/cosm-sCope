#include "render/datalab_renderer_backend.h"

#include <SDL2/SDL_vulkan.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vk_renderer.h"
#include "vk_runtime.h"

enum {
    DATALAB_CANVAS_WIDTH = 4096,
    DATALAB_CANVAS_HEIGHT = 4096
};

typedef struct DatalabRendererBackend {
    SDL_Window *window;
    SDL_Renderer *canvas;
    SDL_Surface *surface;
    VkRenderer vk;
    VkRendererTexture texture;
    DatalabRendererBackendKind kind;
    int vk_initialized;
    int texture_initialized;
    int drawable_width;
    int drawable_height;
    unsigned long frame_count;
} DatalabRendererBackend;

static DatalabRendererBackend g_datalab_backend;

static int datalab_backend_env_enabled(const char *name) {
    const char *value = getenv(name);
    return value && (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
                     strcmp(value, "yes") == 0);
}

static unsigned long datalab_backend_capture_frame(void) {
    const char *value = getenv("DATALAB_VULKAN_CAPTURE_FRAME");
    char *end = NULL;
    unsigned long frame;

    if (!value || value[0] == '\0') {
        return 0u;
    }
    frame = strtoul(value, &end, 10);
    if (!end || end == value || end[0] != '\0') {
        return 0u;
    }
    return frame;
}

static int datalab_backend_vulkan_requested(void) {
    const char *value = getenv("DATALAB_RENDER_BACKEND");
    const char *video_driver = getenv("SDL_VIDEODRIVER");
    if ((video_driver && strcmp(video_driver, "dummy") == 0) ||
        (value && strcmp(value, "sdl") == 0)) {
        return 0;
    }
    return !value || value[0] == '\0' || strcmp(value, "vulkan") == 0;
}

static int datalab_backend_drawable_size(SDL_Window *window, int *width, int *height) {
    if (!window || !width || !height) {
        return 0;
    }
    SDL_Vulkan_GetDrawableSize(window, width, height);
    return *width > 0 && *height > 0 &&
           *width <= DATALAB_CANVAS_WIDTH && *height <= DATALAB_CANVAS_HEIGHT;
}

static int datalab_backend_recreate_presentation(DatalabRendererBackend *backend,
                                                 int width,
                                                 int height) {
    if (!backend || !backend->vk_initialized || width < 1 || height < 1) {
        return 0;
    }
    vk_renderer_wait_idle(&backend->vk);
    if (backend->texture_initialized) {
        vk_renderer_texture_destroy(&backend->vk, &backend->texture);
        memset(&backend->texture, 0, sizeof(backend->texture));
        backend->texture_initialized = 0;
    }
    if (vk_renderer_recreate_swapchain(&backend->vk, backend->window) != VK_SUCCESS) {
        return 0;
    }
    if (vk_renderer_upload_sdl_surface_with_filter(&backend->vk,
                                                   backend->surface,
                                                   &backend->texture,
                                                   VK_FILTER_NEAREST) != VK_SUCCESS) {
        return 0;
    }
    backend->texture_initialized = 1;
    backend->drawable_width = width;
    backend->drawable_height = height;
    fprintf(stdout,
            "DATALAB_VULKAN_RESIZE schema=1 status=pass drawable=%dx%d\n",
            width,
            height);
    return 1;
}

static int datalab_backend_sync_size(DatalabRendererBackend *backend) {
    int width = 0;
    int height = 0;
    if (!backend || backend->kind != DATALAB_RENDERER_BACKEND_VULKAN ||
        !datalab_backend_drawable_size(backend->window, &width, &height)) {
        return 0;
    }
    if (!backend->texture_initialized || width != backend->drawable_width ||
        height != backend->drawable_height) {
        return datalab_backend_recreate_presentation(backend, width, height);
    }
    return 1;
}

uint32_t datalab_renderer_backend_window_flags(void) {
    uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (datalab_backend_vulkan_requested()) {
        flags |= SDL_WINDOW_VULKAN;
    }
    return flags;
}

SDL_Renderer *datalab_renderer_backend_create(SDL_Window *window) {
    DatalabRendererBackend *backend = &g_datalab_backend;
    const int requested_vulkan = datalab_backend_vulkan_requested();
    const int require_vulkan = datalab_backend_env_enabled("DATALAB_REQUIRE_VULKAN");
    const char *driver = SDL_GetCurrentVideoDriver();

    if (!window || backend->canvas) {
        return NULL;
    }
    memset(backend, 0, sizeof(*backend));
    backend->window = window;
    if (!requested_vulkan || (driver && strcmp(driver, "dummy") == 0)) {
        backend->canvas = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!backend->canvas) {
            backend->canvas = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (!backend->canvas || require_vulkan) {
            if (backend->canvas) {
                SDL_DestroyRenderer(backend->canvas);
            }
            memset(backend, 0, sizeof(*backend));
            return NULL;
        }
        backend->kind = DATALAB_RENDERER_BACKEND_SDL;
        fprintf(stdout, "DATALAB_RENDERER_BACKEND schema=1 backend=sdl status=ready\n");
        return backend->canvas;
    }

    backend->surface = SDL_CreateRGBSurfaceWithFormat(0,
                                                      DATALAB_CANVAS_WIDTH,
                                                      DATALAB_CANVAS_HEIGHT,
                                                      32,
                                                      SDL_PIXELFORMAT_ABGR8888);
    if (backend->surface) {
        backend->canvas = SDL_CreateSoftwareRenderer(backend->surface);
    }
    if (backend->canvas) {
        VkRendererConfig config;
        vk_renderer_config_set_defaults(&config);
        config.enable_validation = datalab_backend_env_enabled(
                                       "DATALAB_REQUIRE_VK_VALIDATION")
                                       ? VK_TRUE
                                       : VK_FALSE;
        if (vk_renderer_init(&backend->vk, window, &config) == VK_SUCCESS) {
            backend->vk_initialized = 1;
            backend->kind = DATALAB_RENDERER_BACKEND_VULKAN;
            if (datalab_backend_sync_size(backend)) {
                fprintf(stdout,
                        "DATALAB_RENDERER_BACKEND schema=1 backend=vulkan status=ready drawable=%dx%d canvas=%dx%d\n",
                        backend->drawable_width,
                        backend->drawable_height,
                        DATALAB_CANVAS_WIDTH,
                        DATALAB_CANVAS_HEIGHT);
                return backend->canvas;
            }
        }
    }
    if (backend->vk_initialized) {
        vk_renderer_shutdown(&backend->vk);
    }
    if (backend->canvas) {
        SDL_DestroyRenderer(backend->canvas);
    }
    if (backend->surface) {
        SDL_FreeSurface(backend->surface);
    }
    memset(backend, 0, sizeof(*backend));
    return NULL;
}

void datalab_renderer_backend_destroy(SDL_Renderer *renderer) {
    DatalabRendererBackend *backend = &g_datalab_backend;
    if (!renderer || renderer != backend->canvas) {
        return;
    }
    if (backend->vk_initialized) {
        vk_renderer_wait_idle(&backend->vk);
        if (backend->texture_initialized) {
            vk_renderer_texture_destroy(&backend->vk, &backend->texture);
        }
        vk_renderer_shutdown(&backend->vk);
    }
    SDL_DestroyRenderer(backend->canvas);
    if (backend->surface) {
        SDL_FreeSurface(backend->surface);
    }
    fprintf(stdout,
            "DATALAB_RENDERER_SHUTDOWN schema=1 backend=%s frames=%lu status=pass\n",
            backend->kind == DATALAB_RENDERER_BACKEND_VULKAN ? "vulkan" : "sdl",
            backend->frame_count);
    memset(backend, 0, sizeof(*backend));
}

DatalabRendererBackendKind datalab_renderer_backend_kind(SDL_Renderer *renderer) {
    return renderer && renderer == g_datalab_backend.canvas
               ? g_datalab_backend.kind
               : DATALAB_RENDERER_BACKEND_SDL;
}

int datalab_renderer_backend_output_size(SDL_Renderer *renderer, int *width, int *height) {
    DatalabRendererBackend *backend = &g_datalab_backend;
    if (!renderer || !width || !height) {
        return -1;
    }
    if (renderer != backend->canvas || backend->kind == DATALAB_RENDERER_BACKEND_SDL) {
        return SDL_GetRendererOutputSize(renderer, width, height);
    }
    if (!datalab_backend_drawable_size(backend->window, width, height)) {
        return -1;
    }
    return 0;
}

int datalab_renderer_backend_present(SDL_Renderer *renderer) {
    DatalabRendererBackend *backend = &g_datalab_backend;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent = {0};
    SDL_Rect source;
    SDL_Rect destination;
    VkResult result;

    if (!renderer || renderer != backend->canvas) {
        return 0;
    }
    if (backend->kind == DATALAB_RENDERER_BACKEND_SDL) {
        SDL_RenderPresent(renderer);
        backend->frame_count += 1u;
        return 1;
    }
    if (!datalab_backend_sync_size(backend)) {
        return 0;
    }
    if (backend->frame_count == datalab_backend_capture_frame()) {
        const char *automatic_capture = getenv("DATALAB_VULKAN_CAPTURE");
        if (automatic_capture && automatic_capture[0] &&
            vk_renderer_request_capture(&backend->vk, automatic_capture) != VK_SUCCESS) {
            return 0;
        }
    }
    if (vk_renderer_texture_update_rgba_subrect(&backend->vk,
                                                &backend->texture,
                                                backend->surface->pixels,
                                                (size_t)backend->surface->pitch,
                                                0u,
                                                0u,
                                                (uint32_t)backend->drawable_width,
                                                (uint32_t)backend->drawable_height) != VK_SUCCESS) {
        return 0;
    }
    result = vk_renderer_begin_frame(&backend->vk, &command, &framebuffer, &extent);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        if (!datalab_backend_recreate_presentation(backend,
                                                   backend->drawable_width,
                                                   backend->drawable_height)) {
            return 0;
        }
        result = vk_renderer_begin_frame(&backend->vk, &command, &framebuffer, &extent);
    }
    if (result != VK_SUCCESS || command == VK_NULL_HANDLE ||
        framebuffer == VK_NULL_HANDLE || extent.width == 0u || extent.height == 0u) {
        return 0;
    }
    source = (SDL_Rect){0, 0, backend->drawable_width, backend->drawable_height};
    destination = (SDL_Rect){0, 0, (int)extent.width, (int)extent.height};
    vk_renderer_set_logical_size(&backend->vk, (float)extent.width, (float)extent.height);
    vk_renderer_set_draw_color(&backend->vk, 1.0f, 1.0f, 1.0f, 1.0f);
    vk_renderer_draw_texture(&backend->vk, &backend->texture, &source, &destination);
    result = vk_renderer_end_frame(&backend->vk, command);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return 0;
    }
    backend->frame_count += 1u;
    return 1;
}

int datalab_renderer_backend_request_capture(SDL_Renderer *renderer, const char *path) {
    DatalabRendererBackend *backend = &g_datalab_backend;
    if (!renderer || renderer != backend->canvas ||
        backend->kind != DATALAB_RENDERER_BACKEND_VULKAN || !path || !path[0]) {
        return 0;
    }
    if (!datalab_backend_sync_size(backend)) {
        return 0;
    }
    return vk_renderer_request_capture(&backend->vk, path) == VK_SUCCESS;
}

int datalab_renderer_backend_verify(SDL_Renderer *renderer,
                                    const char *stage,
                                    int require_validation) {
    DatalabRendererBackend *backend = &g_datalab_backend;
    const VkRendererDevice *device;
    const VkRuntimeCapabilityReport *report;
    if (!renderer || renderer != backend->canvas ||
        backend->kind != DATALAB_RENDERER_BACKEND_VULKAN ||
        !backend->vk.context.device) {
        return 0;
    }
    device = backend->vk.context.device;
    report = vk_runtime_get_capability_report(&device->runtime);
    if (!report || report->status != VK_RUNTIME_STATUS_OK || report->device_count == 0u ||
        report->selected_device_index >= report->device_count ||
        device->instance != device->runtime.instance ||
        device->device != device->runtime.device ||
        device->graphics_queue != device->runtime.graphics_queue ||
        device->present_queue != device->runtime.present_queue ||
        (require_validation &&
         (!report->validation_requested || !report->validation_available ||
          !report->validation_enabled || report->validation_load_failed)) ||
        report->validation_warning_count != 0u || report->validation_error_count != 0u) {
        fprintf(stderr,
                "DATALAB_VULKAN_RUNTIME schema=1 stage=%s status=fail\n",
                stage ? stage : "unknown");
        return 0;
    }
    fprintf(stdout,
            "DATALAB_VULKAN_RUNTIME schema=1 stage=%s status=pass runtime=%s device=%s validation_requested=%u validation_enabled=%u warnings=%u errors=%u handles=shared\n",
            stage ? stage : "unknown",
            vk_runtime_version_string(),
            report->devices[report->selected_device_index].device_name,
            report->validation_requested,
            report->validation_enabled,
            report->validation_warning_count,
            report->validation_error_count);
    return 1;
}

int datalab_renderer_backend_drawable_metrics(SDL_Renderer *renderer,
                                              int *logical_width,
                                              int *logical_height,
                                              int *drawable_width,
                                              int *drawable_height,
                                              double *scale) {
    DatalabRendererBackend *backend = &g_datalab_backend;
    double scale_x;
    double scale_y;
    if (!renderer || renderer != backend->canvas || !backend->window ||
        !logical_width || !logical_height || !drawable_width || !drawable_height ||
        !scale) {
        return 0;
    }
    SDL_GetWindowSize(backend->window, logical_width, logical_height);
    if (backend->kind == DATALAB_RENDERER_BACKEND_VULKAN) {
        if (!datalab_backend_drawable_size(backend->window, drawable_width, drawable_height)) {
            return 0;
        }
    } else if (SDL_GetRendererOutputSize(renderer, drawable_width, drawable_height) != 0) {
        return 0;
    }
    if (*logical_width < 1 || *logical_height < 1) {
        return 0;
    }
    scale_x = (double)*drawable_width / (double)*logical_width;
    scale_y = (double)*drawable_height / (double)*logical_height;
    if (!isfinite(scale_x) || !isfinite(scale_y) || fabs(scale_x - scale_y) > 0.01) {
        return 0;
    }
    *scale = scale_x;
    return 1;
}
