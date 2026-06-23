package-desktop:
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(PACKAGE_SOURCE_BIN)"
	@echo "Preparing desktop package..."
	@"$(PACKAGE_APP_RM_GUARD)" "$(PACKAGE_APP_DIR)" "$(PACKAGE_APP_NAME)" package
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
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/data/runtime"
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
	@mkdir -p "$(PACKAGE_SELF_TEST_HOME)" "$(PACKAGE_SELF_TEST_TMP)"
	@HOME="$(abspath $(PACKAGE_SELF_TEST_HOME))" \
		TMPDIR="$(abspath $(PACKAGE_SELF_TEST_TMP))" \
		"$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)" --self-test >"$(PACKAGE_SELF_TEST_OUTPUT)" || (echo "package-desktop self-test failed."; exit 1)
	@! rg -q '/Contents/Resources|/Users/|args=' "$(PACKAGE_SELF_TEST_OUTPUT)" || (echo "package-desktop self-test leaked private paths"; exit 1)
	@rg -q '^LOG_FILE_SCOPE=user-home$$' "$(PACKAGE_SELF_TEST_OUTPUT)" || (echo "package self-test log did not stay under hermetic HOME"; exit 1)
	@rg -q '^DATALAB_RUNTIME_DIR_SCOPE=runtime$$' "$(PACKAGE_SELF_TEST_OUTPUT)" || (echo "package self-test runtime did not stay under hermetic runtime root"; exit 1)
	@rg -q '^APP_DATA_HOME_SCOPE=runtime$$' "$(PACKAGE_SELF_TEST_OUTPUT)" || (echo "package self-test app data did not stay under hermetic runtime root"; exit 1)
	@rg -q '^DATALAB_INPUT_ROOT_SCOPE=runtime$$' "$(PACKAGE_SELF_TEST_OUTPUT)" || (echo "package self-test input root did not stay under runtime root"; exit 1)
	@echo "package-desktop-self-test passed."

test-package-runtime-boundary: package-desktop
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing packaged runtime defaults dir"; exit 1)
	@! find "$(PACKAGE_RESOURCES_DIR)/data/runtime" -type f -print -quit | /usr/bin/grep -q . || (echo "Packaged runtime defaults contain repo-local files"; exit 1)
	@DATALAB_RUNTIME_DIR="$(PACKAGE_RESOURCES_DIR)/data/runtime" \
		DATALAB_INPUT_ROOT="$(PACKAGE_RESOURCES_DIR)/data/runtime" \
		"$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)" --self-test >"$(TARGET_BUILD_DIR)/package_runtime_boundary_self_test.txt"
	@! rg -q '/Contents/Resources|/Users/|args=' "$(TARGET_BUILD_DIR)/package_runtime_boundary_self_test.txt" || (echo "Launcher self-test leaked private paths"; exit 1)
	@rg -q '^DATALAB_RUNTIME_DIR_SCOPE=' "$(TARGET_BUILD_DIR)/package_runtime_boundary_self_test.txt" || (echo "Missing runtime scope in self-test"; exit 1)
	@rg -q '^DATALAB_INPUT_ROOT_SCOPE=runtime$$' "$(TARGET_BUILD_DIR)/package_runtime_boundary_self_test.txt" || (echo "Input root did not stay under mutable runtime root"; exit 1)
	@echo "test-package-runtime-boundary passed."

package-desktop-copy-desktop: package-desktop
	@"$(PACKAGE_APP_RM_GUARD)" "$(DESKTOP_APP_DIR)" "$(PACKAGE_APP_NAME)" desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop package synchronized: $(DESKTOP_APP_DIR)"

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@"$(PACKAGE_APP_RM_GUARD)" "$(DESKTOP_APP_DIR)" "$(PACKAGE_APP_NAME)" desktop
	@rm -rf "$(DESKTOP_APP_DIR)"
	@echo "Removed desktop app copy: $(DESKTOP_APP_DIR)"

package-desktop-refresh: package-desktop
	@"$(PACKAGE_APP_RM_GUARD)" "$(DESKTOP_APP_DIR)" "$(PACKAGE_APP_NAME)" desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

test-package-desktop-path-guard:
	@set -eu; \
	guard="$(PACKAGE_APP_RM_GUARD)"; \
	"$$guard" "build/targets/macOS-arm64/dist/$(PACKAGE_APP_NAME)" "$(PACKAGE_APP_NAME)" package; \
	"$$guard" "$(HOME)/Desktop/$(PACKAGE_APP_NAME)" "$(PACKAGE_APP_NAME)" desktop; \
	if "$$guard" "" "$(PACKAGE_APP_NAME)" package >/dev/null 2>&1; then echo "guard accepted empty path"; exit 1; fi; \
	if "$$guard" "/" "$(PACKAGE_APP_NAME)" package >/dev/null 2>&1; then echo "guard accepted root path"; exit 1; fi; \
	if "$$guard" "$(HOME)" "$(PACKAGE_APP_NAME)" package >/dev/null 2>&1; then echo "guard accepted home path"; exit 1; fi; \
	if "$$guard" "$(HOME)/Desktop" "$(PACKAGE_APP_NAME)" desktop >/dev/null 2>&1; then echo "guard accepted Desktop path"; exit 1; fi; \
	if "$$guard" "$(HOME)/Desktop/not-datalab.app" "$(PACKAGE_APP_NAME)" desktop >/dev/null 2>&1; then echo "guard accepted wrong app name"; exit 1; fi; \
	if "$$guard" "$(HOME)/Desktop/$(PACKAGE_APP_NAME)/../$(PACKAGE_APP_NAME)" "$(PACKAGE_APP_NAME)" desktop >/dev/null 2>&1; then echo "guard accepted traversal path"; exit 1; fi; \
	if "$$guard" "/tmp/$(PACKAGE_APP_NAME)" "$(PACKAGE_APP_NAME)" desktop >/dev/null 2>&1; then echo "guard accepted non-Desktop path"; exit 1; fi; \
	echo "test-package-desktop-path-guard passed."
