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
DATALAB_FOLDER_PICKER_TEST_BIN := $(TEST_BUILD_DIR)/datalab_folder_picker_test
IMAGE_RESIDENCY_TEST_BIN := $(TEST_BUILD_DIR)/datalab_image_residency_contract_test
IMAGE_RESIDENCY_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_image_residency_contract_test.o
RASTER_GENERATION_TEST_BIN := $(TEST_BUILD_DIR)/datalab_raster_generation_contract_test
RASTER_GENERATION_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_raster_generation_contract_test.o
VIEWER_SESSION_TEST_BIN := $(TEST_BUILD_DIR)/datalab_viewer_session_prefs_contract_test
VIEWER_SESSION_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_viewer_session_prefs_contract_test.o
ASYNC_DECODE_TEST_BIN := $(TEST_BUILD_DIR)/datalab_async_decode_contract_test
ASYNC_DECODE_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_async_decode_contract_test.o
THUMBNAIL_DECODE_TEST_BIN := $(TEST_BUILD_DIR)/datalab_thumbnail_decode_contract_test
THUMBNAIL_DECODE_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_thumbnail_decode_contract_test.o
INPUT_CATALOG_TEST_BIN := $(TEST_BUILD_DIR)/datalab_input_catalog_contract_test
INPUT_CATALOG_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_input_catalog_contract_test.o
FOCUS_WINDOW_TEST_BIN := $(TEST_BUILD_DIR)/datalab_focus_window_contract_test
FOCUS_WINDOW_TEST_OBJ := $(TEST_BUILD_DIR)/datalab_focus_window_contract_test.o
TEST_DEPS := $(APP_CONTRACT_TEST_OBJ:.o=.d) $(AUTHORING_INPUT_TEST_OBJ:.o=.d) \
	$(RASTER_VIEWPORT_TEST_OBJ:.o=.d) $(LOOP_POLICY_TEST_OBJ:.o=.d) \
	$(PANEL_POLICY_TEST_OBJ:.o=.d) $(PROFILE_INTERACTION_TEST_OBJ:.o=.d) $(IMAGE_RESIDENCY_TEST_OBJ:.o=.d) $(VIEWER_SESSION_TEST_OBJ:.o=.d) \
	$(ASYNC_DECODE_TEST_OBJ:.o=.d) $(THUMBNAIL_DECODE_TEST_OBJ:.o=.d) $(INPUT_CATALOG_TEST_OBJ:.o=.d) $(FOCUS_WINDOW_TEST_OBJ:.o=.d)
DEPS += $(TEST_DEPS)
-include $(TEST_DEPS)
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
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -DDATALAB_TEST_DEFAULT_PACK=\"$(abspath $(DEFAULT_PACK_SRC))\" -c $< -o $@

$(APP_CONTRACT_TEST_BIN): $(APP_CONTRACT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(APP_CONTRACT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(AUTHORING_INPUT_TEST_OBJ): tests/datalab_authoring_input_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(AUTHORING_INPUT_TEST_BIN): $(AUTHORING_INPUT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(AUTHORING_INPUT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(RASTER_VIEWPORT_TEST_OBJ): tests/datalab_raster_viewport_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(RASTER_VIEWPORT_TEST_BIN): $(RASTER_VIEWPORT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(RASTER_VIEWPORT_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(LOOP_POLICY_TEST_OBJ): tests/datalab_loop_policy_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(LOOP_POLICY_TEST_BIN): $(LOOP_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(LOOP_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(PANEL_POLICY_TEST_OBJ): tests/datalab_panel_policy_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(PANEL_POLICY_TEST_BIN): $(PANEL_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(PANEL_POLICY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(PROFILE_INTERACTION_TEST_OBJ): tests/datalab_profile_interaction_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(PROFILE_INTERACTION_TEST_BIN): $(PROFILE_INTERACTION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(PROFILE_INTERACTION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(DATALAB_FOLDER_PICKER_TEST_BIN): tests/datalab_folder_picker_test.c src/platform/datalab_folder_picker.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -DDATALAB_FOLDER_PICKER_FORCE_LINUX -o $@ tests/datalab_folder_picker_test.c src/platform/datalab_folder_picker.c

$(IMAGE_RESIDENCY_TEST_OBJ): tests/datalab_image_residency_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(IMAGE_RESIDENCY_TEST_BIN): $(IMAGE_RESIDENCY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(IMAGE_RESIDENCY_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(RASTER_GENERATION_TEST_OBJ): tests/datalab_raster_generation_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(RASTER_GENERATION_TEST_BIN): $(RASTER_GENERATION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(RASTER_GENERATION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(VIEWER_SESSION_TEST_OBJ): tests/datalab_viewer_session_prefs_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(VIEWER_SESSION_TEST_BIN): $(VIEWER_SESSION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(VIEWER_SESSION_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(ASYNC_DECODE_TEST_OBJ): tests/datalab_async_decode_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(ASYNC_DECODE_TEST_BIN): $(ASYNC_DECODE_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(ASYNC_DECODE_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(THUMBNAIL_DECODE_TEST_OBJ): tests/datalab_thumbnail_decode_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(THUMBNAIL_DECODE_TEST_BIN): $(THUMBNAIL_DECODE_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(THUMBNAIL_DECODE_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(INPUT_CATALOG_TEST_OBJ): tests/datalab_input_catalog_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(INPUT_CATALOG_TEST_BIN): $(INPUT_CATALOG_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(INPUT_CATALOG_TEST_OBJ) $(filter-out $(PROGRAM_OBJ_DIR)/main.o,$(OBJS)) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

$(FOCUS_WINDOW_TEST_OBJ): tests/datalab_focus_window_contract_test.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(FOCUS_WINDOW_TEST_BIN): $(FOCUS_WINDOW_TEST_OBJ) $(PROGRAM_OBJ_DIR)/app/datalab_focus_window.o
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(FOCUS_WINDOW_TEST_OBJ) $(PROGRAM_OBJ_DIR)/app/datalab_focus_window.o

$(DEFAULT_PACK): $(DEFAULT_PACK_SRC)
	@mkdir -p $(dir $@)
	cp $(DEFAULT_PACK_SRC) $@

test-smoke: $(TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(TEST_BIN)

test-pack-loader: $(PACK_LOADER_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(PACK_LOADER_TEST_BIN)

test-app-contract: $(APP_CONTRACT_TEST_BIN)
	@set -eu; \
		runtime_root="$$(mktemp -d /private/tmp/datalab-app-contract.XXXXXX)" || exit $$?; \
		trap 'rm -rf "$$runtime_root"' EXIT; \
		trap 'exit 1' HUP INT TERM; \
		source_root="$(abspath $(CURDIR))"; \
		test -d "$$source_root"; \
		test -x "$(abspath $(APP_CONTRACT_TEST_BIN))"; \
		cd "$$runtime_root"; \
		printf 'datalab app contract isolated runtime: %s\n' "$$runtime_root"; \
		DATALAB_TEST_RUNTIME_ROOT="$$runtime_root" DATALAB_TEST_SOURCE_ROOT="$$source_root" \
			env -u TARGET_CONTRACT_HELPER "$(abspath $(APP_CONTRACT_TEST_BIN))"

test-authoring-input-contract: $(AUTHORING_INPUT_TEST_BIN)
	@set -eu; \
		runtime_root="$$(mktemp -d /private/tmp/datalab-authoring-contract.XXXXXX)" || exit $$?; \
		trap 'rm -rf "$$runtime_root"' EXIT; \
		trap 'exit 1' HUP INT TERM; \
		source_root="$(abspath $(CURDIR))"; \
		test -d "$$source_root"; \
		test -x "$(abspath $(AUTHORING_INPUT_TEST_BIN))"; \
		cd "$$runtime_root"; \
		printf 'datalab authoring contract isolated runtime: %s\n' "$$runtime_root"; \
		DATALAB_TEST_RUNTIME_ROOT="$$runtime_root" DATALAB_TEST_SOURCE_ROOT="$$source_root" \
			env -u TARGET_CONTRACT_HELPER "$(abspath $(AUTHORING_INPUT_TEST_BIN))"

test-raster-viewport-contract: $(RASTER_VIEWPORT_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(RASTER_VIEWPORT_TEST_BIN)

test-loop-policy-contract: $(LOOP_POLICY_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(LOOP_POLICY_TEST_BIN)

test-panel-policy-contract: $(PANEL_POLICY_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(PANEL_POLICY_TEST_BIN)

test-profile-interaction-contract: $(PROFILE_INTERACTION_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(PROFILE_INTERACTION_TEST_BIN)

test-datalab-folder-picker: $(DATALAB_FOLDER_PICKER_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(DATALAB_FOLDER_PICKER_TEST_BIN)

test-image-residency-contract: $(IMAGE_RESIDENCY_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(IMAGE_RESIDENCY_TEST_BIN)

test-raster-generation-contract: $(RASTER_GENERATION_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(RASTER_GENERATION_TEST_BIN)

test-viewer-session-prefs-contract: $(VIEWER_SESSION_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(VIEWER_SESSION_TEST_BIN)

test-async-decode-contract: $(ASYNC_DECODE_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(ASYNC_DECODE_TEST_BIN)

test-thumbnail-decode-contract: $(THUMBNAIL_DECODE_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(THUMBNAIL_DECODE_TEST_BIN)

test-input-catalog-contract: $(INPUT_CATALOG_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(INPUT_CATALOG_TEST_BIN)

test-focus-window-contract: $(FOCUS_WINDOW_TEST_BIN)
	env -u TARGET_CONTRACT_HELPER ./$(FOCUS_WINDOW_TEST_BIN)

test-w5-acceptance: $(TARGET)
	@set -eu; \
		output_root="$$(mktemp -d /private/tmp/datalab-w5-source.XXXXXX)"; \
		rmdir "$$output_root"; \
		trap 'rm -rf "$$output_root"' EXIT; \
		env -u TARGET_CONTRACT_HELPER "$(TARGET)" --w5-acceptance "$$output_root"; \
		test -f "$$output_root/w5_manifest.txt"; \
		rg -q '^result=pass$$' "$$output_root/w5_manifest.txt"

test-linux-launcher-contract:
	env -u TARGET_CONTRACT_HELPER sh tests/datalab_linux_launcher_contract_test.sh

test-contract: test-app-contract test-authoring-input-contract test-raster-viewport-contract test-loop-policy-contract test-panel-policy-contract test-profile-interaction-contract test-viewer-session-prefs-contract test-async-decode-contract test-thumbnail-decode-contract test-input-catalog-contract test-focus-window-contract

test-package-boundary: test-package-desktop-path-guard test-package-runtime-boundary

test: test-smoke test-pack-loader test-contract test-datalab-folder-picker test-linux-launcher-contract

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
	env -u TARGET_CONTRACT_HELPER "$(TARGET)" --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))" --no-gui

run-headless-smoke: all test-stable $(DEFAULT_PACK)
	env -u TARGET_CONTRACT_HELPER "$(TARGET)" --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))" --no-gui

visual-harness: $(TARGET)
	@echo "visual harness build gate ready: $(TARGET)"
	@echo "launch manual UI validation with: make -C datalab run"

visual-artifact: $(TARGET) $(DEFAULT_PACK)
	@mkdir -p "$(VISUAL_ARTIFACT_ROOT)"
	SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software env -u TARGET_CONTRACT_HELPER "$(TARGET)" --pack "$(if $(PACK),$(PACK),$(DEFAULT_PACK))" --visual-artifact "$(VISUAL_ARTIFACT_OUTPUT)"
	@test -s "$(VISUAL_ARTIFACT_OUTPUT)" || { echo "visual artifact missing or empty: $(VISUAL_ARTIFACT_OUTPUT)" >&2; exit 1; }
	@echo "visual artifact ready: $(VISUAL_ARTIFACT_OUTPUT)"

vulkan-rollout-contract:
	@python3 tools/verify-vulkan-rollout.py --repo-root "$(CURDIR)" \
		--shared-root "$(SHARED_ROOT)" --canonical-shared-root "$(CANONICAL_SHARED_ROOT)"

vulkan-rollout-self-test: $(TARGET) $(DEFAULT_PACK) vulkan-rollout-contract
	@mkdir -p "$(VULKAN_ROLLOUT_DIR)"
	@python3 tools/verify-vulkan-rollout.py --repo-root "$(CURDIR)" \
		--shared-root "$(SHARED_ROOT)" --canonical-shared-root "$(CANONICAL_SHARED_ROOT)" \
		--app "$(abspath $(TARGET))" --shader-root "$(abspath $(VK_RENDERER_DIR))" \
		--default-pack "$(abspath $(DEFAULT_PACK))" \
		--initial-capture "$(VULKAN_ROLLOUT_INITIAL_CAPTURE)" \
		--resized-capture "$(VULKAN_ROLLOUT_RESIZED_CAPTURE)" \
		--log "$(VULKAN_ROLLOUT_LOG)" --minimum-scale 1.5

package-desktop-vulkan-self-test: package-desktop-smoke vulkan-rollout-contract
	@mkdir -p "$(PACKAGE_VULKAN_ROLLOUT_DIR)"
	@python3 tools/verify-vulkan-rollout.py --repo-root "$(CURDIR)" \
		--shared-root "$(SHARED_ROOT)" --canonical-shared-root "$(CANONICAL_SHARED_ROOT)" \
		--app "$(abspath $(PACKAGE_MACOS_DIR)/$(APP_BIN))" \
		--shader-root "$(abspath $(PACKAGE_RESOURCES_DIR)/vk_renderer)" \
		--default-pack "$(abspath $(DEFAULT_PACK_SRC))" \
		--initial-capture "$(PACKAGE_VULKAN_INITIAL_CAPTURE)" \
		--resized-capture "$(PACKAGE_VULKAN_RESIZED_CAPTURE)" \
		--log "$(PACKAGE_VULKAN_LOG)" --minimum-scale 1.5
