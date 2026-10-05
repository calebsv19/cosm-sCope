# DataLab keeps its normal kit archive backend-neutral for the SDL UI.
# Only this native conformance harness enables the optional Vulkan adapter.
RENDER_FIDELITY_BIN := $(HOST_BUILD_DIR)/tests/render-fidelity
RENDER_FIDELITY_OUTPUT_DIR ?= $(TARGET_BUILD_DIR)/render-fidelity
RENDER_FIDELITY_KIT_DIR := $(SHARED_BUILD_DIR)/kit_render_fidelity
RENDER_FIDELITY_KIT_LIB := $(RENDER_FIDELITY_KIT_DIR)/libkit_render.a
RENDER_FIDELITY_SOURCES := $(KIT_RENDER_DIR)/tests/kit_render_fidelity_vk_test.c $(KIT_RENDER_DIR)/tests/kit_render_fidelity_oracle.c
RENDER_FIDELITY_HOST_OBJS := $(VK_RENDERER_OBJS) $(VK_RUNTIME_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(CORE_BASE_OBJS)

.PHONY: render-fidelity-kit-source-check render-fidelity-harness render-fidelity-self-test
render-fidelity-kit-source-check:

$(RENDER_FIDELITY_KIT_LIB): render-fidelity-kit-source-check
	$(MAKE) -C $(KIT_RENDER_DIR) CC="$(HOST_CC) $(ARCH_FLAGS)" PKG_CONFIG="$(PKG_CONFIG)" KIT_RENDER_ENABLE_VK=1 OBJ_DIR="$(abspath $(RENDER_FIDELITY_KIT_DIR))"

# Reuse the native renderer/runtime objects linked by the application.
$(RENDER_FIDELITY_BIN): $(RENDER_FIDELITY_SOURCES) $(KIT_RENDER_DIR)/tests/kit_render_fidelity_fixture.h $(RENDER_FIDELITY_KIT_LIB) $(RENDER_FIDELITY_HOST_OBJS)
	mkdir -p "$(dir $@)"
	$(HOST_CC) $(HOST_CFLAGS) $(RENDER_FIDELITY_SOURCES) $(RENDER_FIDELITY_KIT_LIB) $(RENDER_FIDELITY_HOST_OBJS) $(LIBS) -o $@

render-fidelity-harness: $(RENDER_FIDELITY_BIN)

render-fidelity-self-test: vulkan-rollout-contract render-fidelity-harness
	mkdir -p "$(RENDER_FIDELITY_OUTPUT_DIR)"
	$(if $(filter Darwin,$(UNAME_S)),DYLD_LIBRARY_PATH=$(TARGET_HOMEBREW_PREFIX)/lib,) ./$(RENDER_FIDELITY_BIN) "$(RENDER_FIDELITY_OUTPUT_DIR)"
