package-desktop:
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(PACKAGE_SOURCE_BIN)"
	@echo "Preparing desktop package..."
	@"$(PACKAGE_APP_RM_GUARD)" "$(PACKAGE_APP_DIR)" "$(PACKAGE_APP_NAME)" package
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@sed \
		-e 's/@PACKAGE_DISPLAY_NAME@/$(PACKAGE_DISPLAY_NAME)/g' \
		-e 's/@PACKAGE_BUNDLE_ID@/$(PACKAGE_BUNDLE_ID)/g' \
		-e 's/@PACKAGE_PROFILE@/$(PACKAGE_PROFILE)/g' \
		-e 's/@RUNTIME_NAMESPACE@/$(PACKAGE_RUNTIME_NAMESPACE)/g' \
		-e 's/@LOG_NAMESPACE@/$(PACKAGE_LOG_NAMESPACE)/g' \
		-e 's/@BUILD_LABEL@/$(PACKAGE_BUILD_LABEL)/g' \
		"$(PACKAGE_INFO_PLIST_SRC)" > "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleShortVersionString $(RELEASE_VERSION)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleVersion $(RELEASE_VERSION)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(PACKAGE_MACOS_DIR)/$(APP_BIN)"
	@sed \
		-e 's/@PACKAGE_PROFILE@/$(PACKAGE_PROFILE)/g' \
		-e 's/@RUNTIME_NAMESPACE@/$(PACKAGE_RUNTIME_NAMESPACE)/g' \
		-e 's/@LOG_NAMESPACE@/$(PACKAGE_LOG_NAMESPACE)/g' \
		-e 's/@BUILD_LABEL@/$(PACKAGE_BUILD_LABEL)/g' \
		"$(PACKAGE_LAUNCHER_SRC)" > "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)"
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
	@if [ "$(PACKAGE_EMBED_BUILD_IDENTITY)" = "1" ]; then \
		python3 "$(MEW1_TOOL)" write-identity \
			--output "$(PACKAGE_RESOURCES_DIR)/build_identity.json" \
			--source-root "$(CURDIR)" --binary "$(PACKAGE_MACOS_DIR)/$(APP_BIN)" \
			--profile "$(PACKAGE_PROFILE)" --program datalab --product sCope \
			--version "$(RELEASE_VERSION)" --architecture "$(TARGET_ARCH)" \
			--toolchain "$(PACKAGE_TOOLCHAIN)" --build-label "$(PACKAGE_BUILD_LABEL)"; \
	fi
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/$(APP_BIN)" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_BUNDLE_ID)" || (echo "Bundle identifier mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleName' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_DISPLAY_NAME)" || (echo "Bundle display name mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :DataLabPackageProfile' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_PROFILE)" || (echo "Package profile mismatch"; exit 1)
	@test "$$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(RELEASE_VERSION)" || (echo "Bundle short version mismatch"; exit 1)
	@test "$$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(RELEASE_VERSION)" || (echo "Bundle version mismatch"; exit 1)
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

package-desktop-main-edit:
	@test -f "$(MEW1_TOOL)" || (echo "Missing shared MEW1 helper: $(MEW1_TOOL)"; exit 1)
	@before="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	$(MAKE) package-desktop-smoke \
		DIST_DIR="$(MAIN_EDIT_DIST_DIR)" PACKAGE_APP_NAME="$(MAIN_EDIT_APP_NAME)" \
		PACKAGE_DISPLAY_NAME="$(MAIN_EDIT_DISPLAY_NAME)" PACKAGE_BUNDLE_ID="$(MAIN_EDIT_BUNDLE_ID)" \
		PACKAGE_PROFILE="$(MAIN_EDIT_PROFILE)" PACKAGE_RUNTIME_NAMESPACE="$(MAIN_EDIT_RUNTIME_NAMESPACE)" \
		PACKAGE_LOG_NAMESPACE="$(MAIN_EDIT_LOG_NAMESPACE)" PACKAGE_BUILD_LABEL="$(MAIN_EDIT_BUILD_LABEL)" \
		PACKAGE_EMBED_BUILD_IDENTITY=1 || exit 1; \
	after="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	if [ "$$before" != "$$after" ]; then \
		rm -rf "$(MAIN_EDIT_APP_DIR)"; \
		echo "Source changed during Main Edit packaging; discarded generated package."; \
		exit 1; \
	fi
	@echo "Main Edit desktop package ready: $(MAIN_EDIT_APP_DIR)"

package-desktop-main-edit-self-test: package-desktop-main-edit
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_BUNDLE_ID)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleName' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_DISPLAY_NAME)"
	@test -f "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json"
	@python3 "$(MEW1_TOOL)" verify-identity \
		--identity "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json" \
		--source-root "$(CURDIR)" --binary "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/$(APP_BIN)" \
		--profile "$(MAIN_EDIT_PROFILE)" --program datalab --product sCope \
		--version "$(RELEASE_VERSION)"
	@set -e; \
	fake_home="$(CURDIR)/$(MAIN_EDIT_SELF_TEST_DIR)/home"; \
	rm -rf "$$fake_home"; mkdir -p "$$fake_home"; \
	config="$$(HOME="$$fake_home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/$(LAUNCHER_BIN)" --print-config)"; \
	printf '%s\n' "$$config"; \
	printf '%s\n' "$$config" | grep -Fqx "PACKAGE_PROFILE=$(MAIN_EDIT_PROFILE)"; \
	printf '%s\n' "$$config" | grep -Fqx "RUNTIME_NAMESPACE=$(MAIN_EDIT_RUNTIME_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "LOG_NAMESPACE=$(MAIN_EDIT_LOG_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "BUILD_LABEL=$(MAIN_EDIT_BUILD_LABEL)"; \
	printf '%s\n' "$$config" | grep -Fqx "DATALAB_RUNTIME_DIR=$$fake_home/Library/Application Support/$(MAIN_EDIT_RUNTIME_NAMESPACE)/runtime"; \
	printf '%s\n' "$$config" | grep -Fqx "LOG_FILE=$$fake_home/Library/Logs/$(MAIN_EDIT_LOG_NAMESPACE)/launcher.log"
	@HOME="$(CURDIR)/$(MAIN_EDIT_SELF_TEST_DIR)/home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/$(LAUNCHER_BIN)" --self-test
	@codesign --verify --deep --strict "$(MAIN_EDIT_APP_DIR)"
	@echo "package-desktop-main-edit-self-test passed."

package-desktop-main-edit-refresh: package-desktop-main-edit-self-test
	@test "$(MAIN_EDIT_DESKTOP_APP_DIR)" != "$(HOME)/Desktop/sCope.app" || (echo "Refusing canonical Desktop destination"; exit 1)
	@mkdir -p "$(dir $(MAIN_EDIT_PROCESS_RECEIPT))"
	@python3 "$(MEW1_TOOL)" process-audit --match "$(MAIN_EDIT_DISPLAY_NAME).app" --path "$(MAIN_EDIT_DESKTOP_APP_DIR)" > "$(MAIN_EDIT_PROCESS_RECEIPT)"
	@if grep -Fq '"running": true' "$(MAIN_EDIT_PROCESS_RECEIPT)"; then \
		echo "Refusing to replace a running $(MAIN_EDIT_APP_NAME); process receipt: $(MAIN_EDIT_PROCESS_RECEIPT)"; exit 1; \
	fi
	@mkdir -p "$$(dirname "$(MAIN_EDIT_DESKTOP_APP_DIR)")"
	@rm -rf "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(MAIN_EDIT_APP_DIR)" "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@echo "Refreshed $(MAIN_EDIT_APP_NAME) at $(MAIN_EDIT_DESKTOP_APP_DIR)"

main-edit-package-contract-checks:
	@./tests/run_main_edit_package_contract_checks.sh

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
