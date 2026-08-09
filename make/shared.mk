CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_VIEWPORT2D_DIR := $(SHARED_ROOT)/core/core_viewport2d
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
CORE_LAYOUT_DIR := $(SHARED_ROOT)/core/core_layout
CORE_PANE_DIR := $(SHARED_ROOT)/core/core_pane
KIT_VIZ_DIR := $(SHARED_ROOT)/kit/kit_viz
KIT_GRAPH_TS_DIR := $(SHARED_ROOT)/kit/kit_graph_timeseries
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
VK_RENDERER_DIR := $(SHARED_ROOT)/vk_renderer
VK_RUNTIME_DIR := $(SHARED_ROOT)/vk_runtime
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
KIT_WORKSPACE_AUTHORING_DIR := $(SHARED_ROOT)/kit/kit_workspace_authoring
KIT_GRAPH_TS_LIB := $(KIT_GRAPH_TS_BUILD_DIR)/libkit_graph_timeseries.a
KIT_WORKSPACE_AUTHORING_LIB := $(KIT_WORKSPACE_AUTHORING_BUILD_DIR)/libkit_workspace_authoring.a
KIT_RENDER_LIB := $(KIT_RENDER_BUILD_DIR)/libkit_render.a
VK_RUNTIME_VERSION := $(shell cat $(VK_RUNTIME_DIR)/VERSION 2>/dev/null)

$(KIT_GRAPH_TS_LIB):
	@mkdir -p "$(dir $@)"
	$(MAKE) -C $(KIT_GRAPH_TS_DIR) \
		CC="$(HOST_CC) $(ARCH_FLAGS)" \
		PKG_CONFIG="$(PKG_CONFIG)" \
		OBJ_DIR="$(abspath $(KIT_GRAPH_TS_BUILD_DIR))"

$(KIT_WORKSPACE_AUTHORING_LIB):
	@mkdir -p "$(dir $@)"
	$(MAKE) -C $(KIT_WORKSPACE_AUTHORING_DIR) \
		CC="$(HOST_CC) $(ARCH_FLAGS)" \
		OBJ_DIR="$(abspath $(KIT_WORKSPACE_AUTHORING_BUILD_DIR))"

$(KIT_RENDER_LIB):
	@mkdir -p "$(dir $@)"
	$(MAKE) -C $(KIT_RENDER_DIR) \
		CC="$(HOST_CC) $(ARCH_FLAGS)" \
		OBJ_DIR="$(abspath $(KIT_RENDER_BUILD_DIR))"
