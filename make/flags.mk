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
PNG_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags libpng 2>/dev/null)
PNG_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs libpng 2>/dev/null)

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

ifeq ($(strip $(PNG_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/png.h),)
PNG_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/png.h),)
PNG_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include
else
PNG_CFLAGS := -I/usr/include
endif
endif

ifeq ($(strip $(PNG_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libpng.dylib),)
PNG_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lpng
else ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libpng16.dylib),)
PNG_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lpng16
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libpng.dylib),)
PNG_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lpng
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libpng16.dylib),)
PNG_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lpng16
else
PNG_LIBS := -lpng
endif
endif
endif

COMMON_CFLAGS := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_VIEWPORT2D_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include \
	-I$(CORE_LAYOUT_DIR)/include -I$(CORE_PANE_DIR)/include \
	-I$(CORE_PACK_DIR)/include -I$(KIT_VIZ_DIR)/include -I$(KIT_GRAPH_TS_DIR)/include \
	-I$(KIT_RENDER_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include \
	-I$(KIT_WORKSPACE_AUTHORING_DIR)/include -I$(KIT_UI_DIR)/include \
	$(SDL_CFLAGS) $(SDL_TTF_CFLAGS) $(PNG_CFLAGS)
PROGRAM_CFLAGS := $(COMMON_CFLAGS)
ifeq ($(BUILD_TOOLCHAIN),clang)
PROGRAM_CFLAGS += $(ARCH_FLAGS)
endif
HOST_CFLAGS := $(COMMON_CFLAGS) $(ARCH_FLAGS)

LIBS := -lm $(SDL_LIBS) $(SDL_TTF_LIBS) $(PNG_LIBS)
LDFLAGS :=
