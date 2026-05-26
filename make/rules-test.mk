TEST_BUILD_DIR := $(HOST_BUILD_DIR)/tests
TEST_BIN := $(TEST_BUILD_DIR)/datalab_smoke_test
PACK_LOADER_TEST_BIN := $(TEST_BUILD_DIR)/datalab_pack_loader_test
APP_CONTRACT_TEST_BIN := $(TEST_BUILD_DIR)/datalab_app_contract_test
APP_CONTRACT_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_app_contract_test.o
AUTHORING_INPUT_TEST_BIN := $(TEST_BUILD_DIR)/datalab_authoring_input_contract_test
AUTHORING_INPUT_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_authoring_input_contract_test.o
RASTER_VIEWPORT_TEST_BIN := $(TEST_BUILD_DIR)/datalab_raster_viewport_contract_test
RASTER_VIEWPORT_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_raster_viewport_contract_test.o
LOOP_POLICY_TEST_BIN := $(TEST_BUILD_DIR)/datalab_loop_policy_contract_test
LOOP_POLICY_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_loop_policy_contract_test.o
PANEL_POLICY_TEST_BIN := $(TEST_BUILD_DIR)/datalab_panel_policy_contract_test
PANEL_POLICY_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_panel_policy_contract_test.o
PROFILE_INTERACTION_TEST_BIN := $(TEST_BUILD_DIR)/datalab_profile_interaction_contract_test
PROFILE_INTERACTION_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_profile_interaction_contract_test.o
DEFAULT_PACK_SRC := $(SHARED_ROOT)/core/core_pack/tests/fixtures/physics_v1_sample.pack
DEFAULT_PACK := $(HOST_BUILD_DIR)/default_preview.pack

$(TEST_BIN): tests/datalab_smoke_test.c src/data/dataset_builders.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/datalab_smoke_test.c src/data/dataset_builders.c \
		$(CORE_DATA_DIR)/src/core_data.c $(CORE_BASE_DIR)/src/core_base.c -lm

$(PACK_LOADER_TEST_BIN): tests/datalab_pack_loader_test.c src/data/pack_loader.c src/data/pack_loader_sketch.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/datalab_pack_loader_test.c src/data/pack_loader.c src/data/pack_loader_sketch.c \
		$(CORE_PACK_DIR)/src/core_pack.c $(CORE_IO_DIR)/src/core_io.c $(CORE_BASE_DIR)/src/core_base.c -lm

$(APP_CONTRACT_TEST_OBJ): tests/datalab_app_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -DDATALAB_TEST_DEFAULT_PACK=\"$(abspath $(DEFAULT_PACK_SRC))\" -c $< -o $@

$(APP_CONTRACT_TEST_BIN): $(APP_CONTRACT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(APP_CONTRACT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(AUTHORING_INPUT_TEST_OBJ): tests/datalab_authoring_input_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(AUTHORING_INPUT_TEST_BIN): $(AUTHORING_INPUT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(AUTHORING_INPUT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(RASTER_VIEWPORT_TEST_OBJ): tests/datalab_raster_viewport_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(RASTER_VIEWPORT_TEST_BIN): $(RASTER_VIEWPORT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(RASTER_VIEWPORT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(LOOP_POLICY_TEST_OBJ): tests/datalab_loop_policy_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(LOOP_POLICY_TEST_BIN): $(LOOP_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(LOOP_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(PANEL_POLICY_TEST_OBJ): tests/datalab_panel_policy_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(PANEL_POLICY_TEST_BIN): $(PANEL_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(PANEL_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(PROFILE_INTERACTION_TEST_OBJ): tests/datalab_profile_interaction_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(PROFILE_INTERACTION_TEST_BIN): $(PROFILE_INTERACTION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(PROFILE_INTERACTION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(DEFAULT_PACK): $(DEFAULT_PACK_SRC)
	@mkdir -p $(dir $@)
	cp $(DEFAULT_PACK_SRC) $@

test: $(TEST_BIN) $(PACK_LOADER_TEST_BIN) $(APP_CONTRACT_TEST_BIN) $(AUTHORING_INPUT_TEST_BIN) $(RASTER_VIEWPORT_TEST_BIN) $(LOOP_POLICY_TEST_BIN) $(PANEL_POLICY_TEST_BIN) $(PROFILE_INTERACTION_TEST_BIN)
	./$(TEST_BIN)
	./$(PACK_LOADER_TEST_BIN)
	./$(APP_CONTRACT_TEST_BIN)
	./$(AUTHORING_INPUT_TEST_BIN)
	./$(RASTER_VIEWPORT_TEST_BIN)
	./$(LOOP_POLICY_TEST_BIN)
	./$(PANEL_POLICY_TEST_BIN)
	./$(PROFILE_INTERACTION_TEST_BIN)

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
