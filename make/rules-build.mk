all: $(TARGET)

$(TARGET): $(OBJS) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $(OBJS) $(CORE_OBJS) $(KIT_GRAPH_TS_LIB) $(KIT_WORKSPACE_AUTHORING_LIB) $(KIT_RENDER_LIB) $(LIBS)

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

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
