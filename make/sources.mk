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
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
KIT_UI_SRCS := \
	$(KIT_UI_DIR)/src/kit_ui.c \
	$(KIT_UI_DIR)/src/kit_ui_button.c \
	$(KIT_UI_DIR)/src/kit_ui_hud.c \
	$(KIT_UI_DIR)/src/kit_ui_sdl.c

CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_io/%.o,$(CORE_IO_SRCS))
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_base/%.o,$(CORE_BASE_SRCS))
CORE_VIEWPORT2D_OBJS := $(patsubst $(CORE_VIEWPORT2D_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_viewport2d/%.o,$(CORE_VIEWPORT2D_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_data/%.o,$(CORE_DATA_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_font/%.o,$(CORE_FONT_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_PANE_OBJS := $(patsubst $(CORE_PANE_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/core/core_pane/%.o,$(CORE_PANE_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/kit/kit_viz/%.o,$(KIT_VIZ_SRCS))
KIT_UI_OBJS := $(patsubst $(KIT_UI_DIR)/src/%.c,$(HOST_BUILD_DIR)/shared/kit/kit_ui/%.o,$(KIT_UI_SRCS))
CORE_OBJS := $(CORE_PACK_OBJS) $(CORE_IO_OBJS) $(CORE_BASE_OBJS) $(CORE_VIEWPORT2D_OBJS) $(CORE_DATA_OBJS) $(CORE_FONT_OBJS) $(CORE_THEME_OBJS) $(CORE_PANE_OBJS) $(KIT_VIZ_OBJS) $(KIT_UI_OBJS)
DEPS := $(OBJS:.o=.d)
DEPS += $(CORE_OBJS:.o=.d)
