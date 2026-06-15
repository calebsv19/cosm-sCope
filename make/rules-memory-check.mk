# =========================
#  fisiCs memory-check audit
# =========================

MEMORY_CHECK_FISICS_OVERLAY := physics-units,memory-check
MEMORY_CHECK_REPORT_DIR := build/memory_check
MEMORY_CHECK_STDOUT := $(MEMORY_CHECK_REPORT_DIR)/datalab.stdout
MEMORY_CHECK_STDERR := $(MEMORY_CHECK_REPORT_DIR)/datalab.stderr
MEMORY_CHECK_OBJ_DIR := $(TARGET_BUILD_DIR)/toolchains/fisics/memory_check_obj
MEMORY_CHECK_BIN := $(TARGET_BUILD_DIR)/toolchains/fisics/bin/datalab_memory_check_pack_loader_test
MEMORY_CHECK_SRCS := \
	tests/datalab_pack_loader_test.c \
	src/data/pack_loader.c \
	src/data/pack_loader_sketch.c
MEMORY_CHECK_OBJS := $(patsubst %.c,$(MEMORY_CHECK_OBJ_DIR)/%.o,$(MEMORY_CHECK_SRCS))
MEMORY_CHECK_REPORT_POLICY ?= always
FISICS_MEMCHECK_RUNTIME ?= /Users/calebsv/Desktop/CodeWork/fisiCs/build/unsanitized/libfisics_memcheck_runtime.a
FISICS_MEMCHECK_LINK_LIBS ?=

$(MEMORY_CHECK_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(PROGRAM_CC) $(PROGRAM_CFLAGS) -MMD -MP -c $< -o $@

$(MEMORY_CHECK_BIN): $(MEMORY_CHECK_OBJS) $(CORE_PACK_OBJS) $(CORE_IO_OBJS) $(CORE_BASE_OBJS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(MEMORY_CHECK_OBJS) $(CORE_PACK_OBJS) $(CORE_IO_OBJS) $(CORE_BASE_OBJS) $(FISICS_MEMCHECK_LINK_LIBS) -lm

memory-check-build:
	@$(MAKE) BUILD_TOOLCHAIN=fisics PROGRAM_CC="$(FISICS_CC) --overlay=$(MEMORY_CHECK_FISICS_OVERLAY)" FISICS_MEMCHECK_LINK_LIBS="$(FISICS_MEMCHECK_RUNTIME)" -B "$(MEMORY_CHECK_BIN)"

memory-check-run: memory-check-build
	@mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(MEMORY_CHECK_BIN)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"
	@echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"
	@echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"

memory-check-audit: memory-check-run
	@echo "memory-check summary:"
	@grep -E "\\[fisics:memory-check\\] (summary|leak|double free|unknown pointer free)" "$(MEMORY_CHECK_STDERR)" || true
