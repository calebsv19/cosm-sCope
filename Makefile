CC ?= cc
CSTD ?= -std=c11
WARN ?= -Wall -Wextra -Wpedantic
DEBUG ?= -g

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
TARGET := datalab
SHARED_ROOT ?= third_party/codework_shared

CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
KIT_VIZ_DIR := $(SHARED_ROOT)/kit/kit_viz
KIT_GRAPH_TS_DIR := $(SHARED_ROOT)/kit/kit_graph_timeseries
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)

UNAME_S := $(shell uname -s)

CFLAGS := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include \
	-I$(CORE_PACK_DIR)/include -I$(KIT_VIZ_DIR)/include -I$(KIT_GRAPH_TS_DIR)/include \
	-I$(KIT_RENDER_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include

LIBS := -lm
ifeq ($(UNAME_S),Darwin)
	CFLAGS += -I/opt/homebrew/include -D_THREAD_SAFE
	LDFLAGS += -L/opt/homebrew/lib
	LIBS += -lSDL2
else
	ifneq ($(SDL_CFLAGS),)
		CFLAGS += $(SDL_CFLAGS)
		LIBS += $(SDL_LIBS)
	else
		CFLAGS += -I/usr/include/SDL2
		LIBS += -lSDL2
	endif
endif

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c

CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/shared/core/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/shared/core/core_io/%.o,$(CORE_IO_SRCS))
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/shared/core/core_base/%.o,$(CORE_BASE_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/shared/core/core_data/%.o,$(CORE_DATA_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/shared/kit/kit_viz/%.o,$(KIT_VIZ_SRCS))
CORE_OBJS := $(CORE_PACK_OBJS) $(CORE_IO_OBJS) $(CORE_BASE_OBJS) $(CORE_DATA_OBJS) $(KIT_VIZ_OBJS)
DEPS := $(OBJS:.o=.d)
DEPS += $(CORE_OBJS:.o=.d)

TEST_BIN := $(BUILD_DIR)/datalab_smoke_test
PACK_LOADER_TEST_BIN := $(BUILD_DIR)/datalab_pack_loader_test
KIT_GRAPH_TS_LIB := $(KIT_GRAPH_TS_DIR)/build/libkit_graph_timeseries.a
DEFAULT_PACK_SRC := $(SHARED_ROOT)/core/core_pack/tests/fixtures/physics_v1_sample.pack
DEFAULT_PACK := $(BUILD_DIR)/default_preview.pack

.PHONY: all clean test run run-headless run-headless-smoke visual-harness test-stable test-legacy

all: $(TARGET)

$(TARGET): $(OBJS) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(LIBS)

$(KIT_GRAPH_TS_LIB):
	$(MAKE) -C $(KIT_GRAPH_TS_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/shared/core/core_pack/%.o: $(CORE_PACK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/shared/core/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/shared/core/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/shared/core/core_data/%.o: $(CORE_DATA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/shared/kit/kit_viz/%.o: $(KIT_VIZ_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_BIN): tests/datalab_smoke_test.c src/data/dataset_builders.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/datalab_smoke_test.c src/data/dataset_builders.c \
		$(CORE_DATA_DIR)/src/core_data.c $(CORE_BASE_DIR)/src/core_base.c -lm

$(PACK_LOADER_TEST_BIN): tests/datalab_pack_loader_test.c src/data/pack_loader.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/datalab_pack_loader_test.c src/data/pack_loader.c \
		$(CORE_PACK_DIR)/src/core_pack.c $(CORE_IO_DIR)/src/core_io.c $(CORE_BASE_DIR)/src/core_base.c -lm

$(DEFAULT_PACK): $(DEFAULT_PACK_SRC)
	@mkdir -p $(BUILD_DIR)
	cp $(DEFAULT_PACK_SRC) $@

test: $(TEST_BIN) $(PACK_LOADER_TEST_BIN)
	./$(TEST_BIN)
	./$(PACK_LOADER_TEST_BIN)

test-stable: test

test-legacy:
	@echo "No legacy test lane defined for datalab."

run: $(TARGET) $(DEFAULT_PACK)
	./$(TARGET) --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))"

run-headless: $(TARGET) $(DEFAULT_PACK)
	./$(TARGET) --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))" --no-gui

run-headless-smoke: all test-stable $(DEFAULT_PACK)
	./$(TARGET) --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))" --no-gui

visual-harness: $(TARGET)
	@echo "visual harness build gate ready: $(TARGET)"
	@echo "launch manual UI validation with: make -C datalab run"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
