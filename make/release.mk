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
