SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(PROGRAM_OBJ_DIR)/%.o,$(SRCS))
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_VIEWPORT2D_SRCS := $(CORE_VIEWPORT2D_DIR)/src/core_viewport2d.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_PANE_SRCS := $(CORE_PANE_DIR)/src/core_pane.c
CORE_WORKSPACE_AUTHORING_SESSION_SRCS := $(CORE_WORKSPACE_AUTHORING_SESSION_DIR)/src/core_workspace_authoring_session.c
CORE_QUEUE_SRCS := $(CORE_QUEUE_DIR)/src/core_queue.c
CORE_WAKE_SRCS := $(CORE_WAKE_DIR)/src/core_wake.c
CORE_WORKERS_SRCS := $(CORE_WORKERS_DIR)/src/core_workers.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
KIT_UI_SRCS := \
	$(KIT_UI_DIR)/src/kit_ui.c \
	$(KIT_UI_DIR)/src/kit_ui_button.c \
	$(KIT_UI_DIR)/src/kit_ui_hud.c \
	$(KIT_UI_DIR)/src/kit_ui_sdl.c

VK_RUNTIME_SRCS := \
	$(VK_RUNTIME_DIR)/src/vk_runtime.c \
	$(VK_RUNTIME_DIR)/src/vk_runtime_instance.c \
	$(VK_RUNTIME_DIR)/src/vk_runtime_json.c \
	$(VK_RUNTIME_DIR)/src/vk_runtime_compute.c \
	$(VK_RUNTIME_DIR)/src/vk_runtime_resident.c \
	$(VK_RUNTIME_DIR)/src/vk_runtime_program.c \
	$(VK_RUNTIME_DIR)/src/vk_runtime_timing.c
VK_RENDERER_SRCS := \
	$(VK_RENDERER_DIR)/src/vk_renderer.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_commands.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_config.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_context.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_device.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_lifecycle.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_memory.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_pipeline.c \
	$(VK_RENDERER_DIR)/src/vk_renderer_textures.c

CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_io/%.o,$(CORE_IO_SRCS))
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_base/%.o,$(CORE_BASE_SRCS))
CORE_VIEWPORT2D_OBJS := $(patsubst $(CORE_VIEWPORT2D_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_viewport2d/%.o,$(CORE_VIEWPORT2D_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_data/%.o,$(CORE_DATA_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_font/%.o,$(CORE_FONT_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_PANE_OBJS := $(patsubst $(CORE_PANE_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_pane/%.o,$(CORE_PANE_SRCS))
CORE_WORKSPACE_AUTHORING_SESSION_OBJS := $(patsubst $(CORE_WORKSPACE_AUTHORING_SESSION_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_workspace_authoring_session/%.o,$(CORE_WORKSPACE_AUTHORING_SESSION_SRCS))
CORE_QUEUE_OBJS := $(patsubst $(CORE_QUEUE_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_queue/%.o,$(CORE_QUEUE_SRCS))
CORE_WAKE_OBJS := $(patsubst $(CORE_WAKE_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_wake/%.o,$(CORE_WAKE_SRCS))
CORE_WORKERS_OBJS := $(patsubst $(CORE_WORKERS_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_workers/%.o,$(CORE_WORKERS_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/kit/kit_viz/%.o,$(KIT_VIZ_SRCS))
KIT_UI_OBJS := $(patsubst $(KIT_UI_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/kit/kit_ui/%.o,$(KIT_UI_SRCS))
VK_RUNTIME_OBJS := $(patsubst $(VK_RUNTIME_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/vk_runtime/%.o,$(VK_RUNTIME_SRCS))
VK_RENDERER_OBJS := $(patsubst $(VK_RENDERER_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/vk_renderer/%.o,$(VK_RENDERER_SRCS))
CORE_OBJS := $(CORE_PACK_OBJS) $(CORE_IO_OBJS) $(CORE_BASE_OBJS) $(CORE_VIEWPORT2D_OBJS) $(CORE_DATA_OBJS) $(CORE_FONT_OBJS) $(CORE_THEME_OBJS) $(CORE_PANE_OBJS) $(CORE_WORKSPACE_AUTHORING_SESSION_OBJS) $(CORE_QUEUE_OBJS) $(CORE_WAKE_OBJS) $(CORE_WORKERS_OBJS) $(KIT_VIZ_OBJS) $(KIT_UI_OBJS) $(VK_RUNTIME_OBJS) $(VK_RENDERER_OBJS)
DEPS := $(OBJS:.o=.d)
DEPS += $(CORE_OBJS:.o=.d)
