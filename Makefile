PROGRAM_KEY := datalab
TARGET_NAME := datalab
APP_BIN := datalab-bin
LAUNCHER_BIN := datalab-launcher
PACKAGE_APP_NAME := sCope.app
RELEASE_PRODUCT_NAME := sCope
RELEASE_BUNDLE_ID := com.cosm.scope

HOST_CC ?= cc
FISICS_CC ?= /Users/calebsv/Desktop/CodeWork/fisiCs/fisics
BUILD_TOOLCHAIN ?= clang
PACKAGE_TOOLCHAIN ?= $(BUILD_TOOLCHAIN)
RELEASE_TOOLCHAIN := clang
PKG_CONFIG ?= pkg-config
CSTD ?= -std=c11
WARN ?= -Wall -Wextra -Wpedantic
DEBUG ?= -g
RELEASE_CHANNEL ?= stable
VERSION_FILE ?= VERSION
TARGET_CONTRACT_HELPER ?= ../bin/desktop_release_target_contract.sh

HOST_ARCH := $(strip $(shell "$(TARGET_CONTRACT_HELPER)" get host_arch))
TARGET_OS_INPUT := $(TARGET_OS)
TARGET_ARCH_INPUT := $(TARGET_ARCH)
TARGET_VARIANT_INPUT := $(TARGET_VARIANT)
TARGET_OS ?= $(strip $(shell TARGET_OS="$(TARGET_OS_INPUT)" TARGET_ARCH="$(TARGET_ARCH_INPUT)" TARGET_VARIANT="$(TARGET_VARIANT_INPUT)" "$(TARGET_CONTRACT_HELPER)" get target_os))
TARGET_ARCH ?= $(strip $(shell TARGET_OS="$(TARGET_OS_INPUT)" TARGET_ARCH="$(TARGET_ARCH_INPUT)" TARGET_VARIANT="$(TARGET_VARIANT_INPUT)" "$(TARGET_CONTRACT_HELPER)" get target_arch))
TARGET_VARIANT ?= $(strip $(shell TARGET_OS="$(TARGET_OS_INPUT)" TARGET_ARCH="$(TARGET_ARCH_INPUT)" TARGET_VARIANT="$(TARGET_VARIANT_INPUT)" "$(TARGET_CONTRACT_HELPER)" get target_variant))
TARGET_TRIPLE := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get target_triple))
RELEASE_PLATFORM := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get release_platform))
RELEASE_ARCH := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get release_arch))
TARGET_HOMEBREW_PREFIX ?= $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get homebrew_prefix))
TARGET_ALT_HOMEBREW_PREFIX ?= $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get alt_homebrew_prefix))
TARGET_PKG_CONFIG_LIBDIR ?= $(TARGET_HOMEBREW_PREFIX)/lib/pkgconfig:$(TARGET_HOMEBREW_PREFIX)/share/pkgconfig
TARGET_DEP_SEARCH_ROOTS ?= $(TARGET_HOMEBREW_PREFIX):$(TARGET_ALT_HOMEBREW_PREFIX)
ARCH_FLAGS := -arch $(TARGET_ARCH)

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
TARGET_BUILD_DIR := $(BUILD_DIR)/targets/$(TARGET_TRIPLE)
HOST_BUILD_DIR := $(TARGET_BUILD_DIR)/host
PROGRAM_BUILD_DIR := $(TARGET_BUILD_DIR)/toolchains/$(BUILD_TOOLCHAIN)
PROGRAM_OBJ_DIR := $(PROGRAM_BUILD_DIR)/obj
PROGRAM_BIN_DIR := $(PROGRAM_BUILD_DIR)/bin
TARGET := $(PROGRAM_BIN_DIR)/$(TARGET_NAME)
DIST_DIR := $(TARGET_BUILD_DIR)/dist

SHARED_ROOT ?= third_party/codework_shared
SHARED_BUILD_DIR := $(TARGET_BUILD_DIR)/shared
KIT_GRAPH_TS_BUILD_DIR := $(SHARED_BUILD_DIR)/kit_graph_timeseries
KIT_WORKSPACE_AUTHORING_BUILD_DIR := $(SHARED_BUILD_DIR)/kit_workspace_authoring
KIT_RENDER_BUILD_DIR := $(SHARED_BUILD_DIR)/kit_render

PACKAGE_APP_DIR := $(DIST_DIR)/$(PACKAGE_APP_NAME)
PACKAGE_CONTENTS_DIR := $(PACKAGE_APP_DIR)/Contents
PACKAGE_MACOS_DIR := $(PACKAGE_CONTENTS_DIR)/MacOS
PACKAGE_RESOURCES_DIR := $(PACKAGE_CONTENTS_DIR)/Resources
PACKAGE_FRAMEWORKS_DIR := $(PACKAGE_CONTENTS_DIR)/Frameworks
PACKAGE_INFO_PLIST_SRC := tools/packaging/macos/Info.plist
PACKAGE_LAUNCHER_SRC := tools/packaging/macos/datalab-launcher
PACKAGE_DYLIB_BUNDLER := tools/packaging/macos/bundle-dylibs.sh
PACKAGE_APP_ICON_NAME := AppIcon
PACKAGE_APP_ICON_FILE := $(PACKAGE_APP_ICON_NAME).icns
PACKAGE_LOCAL_ICON_DIR := tools/packaging/macos/local_app_icon
PACKAGE_APP_ICON_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_FILE)
PACKAGE_APP_ICONSET_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_NAME).iconset
PACKAGE_BUNDLED_ICON_PATH := $(PACKAGE_RESOURCES_DIR)/$(PACKAGE_APP_ICON_FILE)
DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(PACKAGE_APP_NAME)
PACKAGE_ADHOC_SIGN_IDENTITY ?= -

RELEASE_VERSION ?= $(strip $(shell cat "$(VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
RELEASE_PROGRAM_KEY := $(PROGRAM_KEY)
RELEASE_ARTIFACT_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(RELEASE_PLATFORM)-$(RELEASE_ARCH)-$(RELEASE_CHANNEL)
RELEASE_DIR := build/release
RELEASE_APP_ZIP := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).zip
RELEASE_APP_ZIP_SHA256 := $(RELEASE_APP_ZIP).sha256
RELEASE_MANIFEST := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).manifest.txt

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
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
KIT_WORKSPACE_AUTHORING_DIR := $(SHARED_ROOT)/kit/kit_workspace_authoring
KIT_GRAPH_TS_LIB := $(KIT_GRAPH_TS_BUILD_DIR)/libkit_graph_timeseries.a
KIT_WORKSPACE_AUTHORING_LIB := $(KIT_WORKSPACE_AUTHORING_BUILD_DIR)/libkit_workspace_authoring.a
KIT_RENDER_LIB := $(KIT_RENDER_BUILD_DIR)/libkit_render.a

ifeq ($(BUILD_TOOLCHAIN),clang)
PROGRAM_CC := $(HOST_CC)
else ifeq ($(BUILD_TOOLCHAIN),fisics)
PROGRAM_CC := $(FISICS_CC)
else
$(error Unsupported BUILD_TOOLCHAIN '$(BUILD_TOOLCHAIN)'; expected clang or fisics)
endif

ifneq ($(filter $(PACKAGE_TOOLCHAIN),clang fisics),$(PACKAGE_TOOLCHAIN))
$(error Unsupported PACKAGE_TOOLCHAIN '$(PACKAGE_TOOLCHAIN)'; expected clang or fisics)
endif

define program_bin_for
$(BUILD_DIR)/targets/$(TARGET_TRIPLE)/toolchains/$(1)/bin/$(TARGET_NAME)
endef

PACKAGE_SOURCE_BIN := $(call program_bin_for,$(PACKAGE_TOOLCHAIN))

SDL_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs sdl2 2>/dev/null)
SDL_TTF_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sdl2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs sdl2_ttf 2>/dev/null)

ifeq ($(strip $(SDL_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/SDL2/SDL.h),)
SDL_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/SDL2/SDL.h),)
SDL_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
else
SDL_CFLAGS := -I/usr/include/SDL2 -D_THREAD_SAFE
endif
endif

ifeq ($(strip $(SDL_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libSDL2.dylib),)
SDL_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lSDL2
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libSDL2.dylib),)
SDL_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lSDL2
else
SDL_LIBS := -lSDL2
endif
endif

ifeq ($(strip $(SDL_TTF_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/SDL2/SDL_ttf.h),)
SDL_TTF_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/SDL2/SDL_ttf.h),)
SDL_TTF_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
else
SDL_TTF_CFLAGS := -I/usr/include/SDL2 -D_THREAD_SAFE
endif
endif

ifeq ($(strip $(SDL_TTF_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libSDL2_ttf.dylib),)
SDL_TTF_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lSDL2_ttf
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libSDL2_ttf.dylib),)
SDL_TTF_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lSDL2_ttf
else
SDL_TTF_LIBS := -lSDL2_ttf
endif
endif

COMMON_CFLAGS := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_VIEWPORT2D_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include \
	-I$(CORE_LAYOUT_DIR)/include -I$(CORE_PANE_DIR)/include \
	-I$(CORE_PACK_DIR)/include -I$(KIT_VIZ_DIR)/include -I$(KIT_GRAPH_TS_DIR)/include \
	-I$(KIT_RENDER_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include \
	-I$(KIT_WORKSPACE_AUTHORING_DIR)/include \
	$(SDL_CFLAGS) $(SDL_TTF_CFLAGS)
PROGRAM_CFLAGS := $(COMMON_CFLAGS)
ifeq ($(BUILD_TOOLCHAIN),clang)
PROGRAM_CFLAGS += $(ARCH_FLAGS)
endif
HOST_CFLAGS := $(COMMON_CFLAGS) $(ARCH_FLAGS)

LIBS := -lm $(SDL_LIBS) $(SDL_TTF_LIBS)
LDFLAGS :=

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(PROGRAM_OBJ_DIR)/%.o,$(SRCS))
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_VIEWPORT2D_SRCS := $(CORE_VIEWPORT2D_DIR)/src/core_viewport2d.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c

CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_io/%.o,$(CORE_IO_SRCS))
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_base/%.o,$(CORE_BASE_SRCS))
CORE_VIEWPORT2D_OBJS := $(patsubst $(CORE_VIEWPORT2D_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_viewport2d/%.o,$(CORE_VIEWPORT2D_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_data/%.o,$(CORE_DATA_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_font/%.o,$(CORE_FONT_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_theme/%.o,$(CORE_THEME_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/kit/kit_viz/%.o,$(KIT_VIZ_SRCS))
CORE_OBJS := $(CORE_PACK_OBJS) $(CORE_IO_OBJS) $(CORE_BASE_OBJS) $(CORE_VIEWPORT2D_OBJS) $(CORE_DATA_OBJS) $(CORE_FONT_OBJS) $(CORE_THEME_OBJS) $(KIT_VIZ_OBJS)
DEPS := $(OBJS:.o=.d)
DEPS += $(CORE_OBJS:.o=.d)

TEST_BUILD_DIR := $(HOST_BUILD_DIR)/tests
TEST_BIN := $(TEST_BUILD_DIR)/datalab_smoke_test
PACK_LOADER_TEST_BIN := $(TEST_BUILD_DIR)/datalab_pack_loader_test
DEFAULT_PACK_SRC := $(SHARED_ROOT)/core/core_pack/tests/fixtures/physics_v1_sample.pack
DEFAULT_PACK := $(HOST_BUILD_DIR)/default_preview.pack

.PHONY: all clean test run run-headless run-headless-smoke visual-harness test-stable test-legacy \
	package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop \
	package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh \
	release-contract release-clean release-build release-bundle-audit release-sign release-verify \
	release-verify-signed release-notarize release-staple release-verify-notarized release-artifact \
	release-distribute release-desktop-refresh

all: $(TARGET)

$(TARGET): $(OBJS) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(OBJS) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

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

$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(PROGRAM_CC) $(PROGRAM_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/core/core_pack/%.o: $(CORE_PACK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/core/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/core/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/core/core_viewport2d/%.o: $(CORE_VIEWPORT2D_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/core/core_data/%.o: $(CORE_DATA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/core/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/core/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD_DIR)/shared/kit/kit_viz/%.o: $(KIT_VIZ_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(TEST_BIN): tests/datalab_smoke_test.c src/data/dataset_builders.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/datalab_smoke_test.c src/data/dataset_builders.c \
		$(CORE_DATA_DIR)/src/core_data.c $(CORE_BASE_DIR)/src/core_base.c -lm

$(PACK_LOADER_TEST_BIN): tests/datalab_pack_loader_test.c src/data/pack_loader.c src/data/pack_loader_sketch.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/datalab_pack_loader_test.c src/data/pack_loader.c src/data/pack_loader_sketch.c \
		$(CORE_PACK_DIR)/src/core_pack.c $(CORE_IO_DIR)/src/core_io.c $(CORE_BASE_DIR)/src/core_base.c -lm

$(DEFAULT_PACK): $(DEFAULT_PACK_SRC)
	@mkdir -p $(dir $@)
	cp $(DEFAULT_PACK_SRC) $@

test: $(TEST_BIN) $(PACK_LOADER_TEST_BIN)
	./$(TEST_BIN)
	./$(PACK_LOADER_TEST_BIN)

test-stable: test

test-legacy:
	@echo "No legacy test lane defined for datalab."

run: $(TARGET)
	@if [ -n "$(PACK)" ]; then \
		"$(TARGET)" --pack "$(PACK)"; \
	else \
		"$(TARGET)"; \
	fi

run-headless: $(TARGET) $(DEFAULT_PACK)
	"$(TARGET)" --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))" --no-gui

run-headless-smoke: all test-stable $(DEFAULT_PACK)
	"$(TARGET)" --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))" --no-gui

visual-harness: $(TARGET)
	@echo "visual harness build gate ready: $(TARGET)"
	@echo "launch manual UI validation with: make -C datalab run"

package-desktop:
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(PACKAGE_SOURCE_BIN)"
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(PACKAGE_MACOS_DIR)/$(APP_BIN)"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)"
	@chmod +x "$(PACKAGE_MACOS_DIR)/$(APP_BIN)" "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)"
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		/usr/bin/iconutil -c icns -o "$(PACKAGE_BUNDLED_ICON_PATH)" "$(PACKAGE_APP_ICONSET_SRC)" || exit 1; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "warning: no app icon source found at $(PACKAGE_APP_ICON_SRC) or $(PACKAGE_APP_ICONSET_SRC)"; \
	fi
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/$(APP_BIN)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/data" "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts"
	@if [ -d "data/runtime" ]; then cp -R data/runtime "$(PACKAGE_RESOURCES_DIR)/data/"; else mkdir -p "$(PACKAGE_RESOURCES_DIR)/data/runtime"; fi
	@cp -R "$(SHARED_ROOT)/assets/fonts/." "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/$(APP_BIN)"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/$(APP_BIN)" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1); \
	fi
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime data dir"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing shared font"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@actual_archs="$$(/usr/bin/lipo -archs "$(PACKAGE_MACOS_DIR)/$(APP_BIN)" 2>/dev/null || true)"; \
	printf '%s\n' "$$actual_archs" | /usr/bin/grep -qw "$(TARGET_ARCH)" || (echo "Unexpected app binary archs: $$actual_archs"; exit 1)
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		dylib_archs="$$(/usr/bin/lipo -archs "$$dylib" 2>/dev/null || true)"; \
		printf '%s\n' "$$dylib_archs" | /usr/bin/grep -qw "$(TARGET_ARCH)" || (echo "Unexpected dylib archs for $$dylib: $$dylib_archs"; exit 1); \
	done
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop package synchronized: $(DESKTOP_APP_DIR)"

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(DESKTOP_APP_DIR)"
	@echo "Removed desktop app copy: $(DESKTOP_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

release-contract:
	@echo "PROGRAM_KEY=$(PROGRAM_KEY)"
	@echo "HOST_ARCH=$(HOST_ARCH)"
	@echo "TARGET_OS=$(TARGET_OS)"
	@echo "TARGET_ARCH=$(TARGET_ARCH)"
	@echo "TARGET_VARIANT=$(TARGET_VARIANT)"
	@echo "TARGET_TRIPLE=$(TARGET_TRIPLE)"
	@echo "RELEASE_PLATFORM=$(RELEASE_PLATFORM)"
	@echo "RELEASE_ARCH=$(RELEASE_ARCH)"
	@echo "TARGET_HOMEBREW_PREFIX=$(TARGET_HOMEBREW_PREFIX)"
	@echo "TARGET_PKG_CONFIG_LIBDIR=$(TARGET_PKG_CONFIG_LIBDIR)"
	@echo "RELEASE_VERSION=$(RELEASE_VERSION)"
	@echo "RELEASE_CHANNEL=$(RELEASE_CHANNEL)"
	@echo "RELEASE_APP_ZIP=$(RELEASE_APP_ZIP)"
	@echo "RELEASE_MANIFEST=$(RELEASE_MANIFEST)"

release-clean:
	@rm -rf "$(RELEASE_DIR)"

release-build:
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" package-desktop
	@mkdir -p "$(RELEASE_DIR)"
	@echo "Release build prepared at $(PACKAGE_APP_DIR)"

release-bundle-audit: release-build
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" package-desktop-smoke
	@/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "Bundle identifier mismatch"; exit 1)
	@otool -L "$(PACKAGE_MACOS_DIR)/$(APP_BIN)" > "$(RELEASE_DIR)/otool_datalab_bin.txt"
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		out="$(RELEASE_DIR)/otool_$$(basename "$$dylib").txt"; \
		otool -L "$$dylib" > "$$out"; \
	done
	@! rg -q '/opt/homebrew|/usr/local|/Users/' "$(RELEASE_DIR)"/otool_*.txt || (echo "Found non-portable dylib linkage"; exit 1)
	@! rg -q '@rpath/' "$(RELEASE_DIR)"/otool_*.txt || (echo "Found unresolved @rpath dylib linkage"; exit 1)
	@"$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)" --print-config > "$(RELEASE_DIR)/print_config.txt"
	@rg -q '^DATALAB_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" || (echo "Missing DATALAB_RUNTIME_DIR in launcher config"; exit 1)
	@rg -q '^VK_ICD_FILENAMES=' "$(RELEASE_DIR)/print_config.txt" || (echo "Missing VK_ICD_FILENAMES in launcher config"; exit 1)
	@runtime_dir="$$(/usr/bin/grep '^DATALAB_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	case "$$runtime_dir" in *"/Contents/Resources"*) echo "runtime dir incorrectly points into app bundle: $$runtime_dir"; exit 1;; esac
	@input_root="$$(/usr/bin/grep '^DATALAB_INPUT_ROOT=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	case "$$input_root" in *"/Contents/Resources"*) echo "input root incorrectly points into app bundle: $$input_root"; exit 1;; esac
	@echo "release-bundle-audit passed."

release-sign: release-build
	@echo "release-sign scaffold: using ad-hoc signed package output from package-desktop."

release-verify: release-bundle-audit
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" package-desktop-self-test
	@echo "release-verify passed."

release-verify-signed: release-verify
	@echo "release-verify-signed scaffold passed."

release-notarize: release-sign
	@echo "release-notarize scaffold placeholder."

release-staple: release-notarize
	@echo "release-staple scaffold placeholder."

release-verify-notarized: release-verify
	@echo "release-verify-notarized scaffold placeholder."

release-artifact: release-verify
	@mkdir -p "$(RELEASE_DIR)"
	@rm -f "$(RELEASE_APP_ZIP)" "$(RELEASE_APP_ZIP_SHA256)" "$(RELEASE_MANIFEST)"
	@cd "$(DIST_DIR)" && zip -qr "$(abspath $(RELEASE_APP_ZIP))" "$(PACKAGE_APP_NAME)"
	@shasum -a 256 "$(RELEASE_APP_ZIP)" > "$(RELEASE_APP_ZIP_SHA256)"
	@{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(PROGRAM_KEY)"; \
		echo "host_arch=$(HOST_ARCH)"; \
		echo "target_os=$(TARGET_OS)"; \
		echo "target_arch=$(TARGET_ARCH)"; \
		echo "target_variant=$(TARGET_VARIANT)"; \
		echo "target_triple=$(TARGET_TRIPLE)"; \
		echo "release_platform=$(RELEASE_PLATFORM)"; \
		echo "release_arch=$(RELEASE_ARCH)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "zip=$(RELEASE_APP_ZIP)"; \
		echo "sha256=$$(cut -d' ' -f1 "$(RELEASE_APP_ZIP_SHA256)")"; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

release-distribute: release-artifact
	@echo "release-distribute scaffold passed."

release-desktop-refresh: release-distribute
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "release-desktop-refresh passed."

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
