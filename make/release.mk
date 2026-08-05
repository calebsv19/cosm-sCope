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
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" package-desktop-self-test
	@mkdir -p "$(RELEASE_DIR)"
	@echo "Release build prepared at $(PACKAGE_APP_DIR)"

release-bundle-audit: release-build
	@/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "Bundle identifier mismatch"; exit 1)
	@/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_short_version.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_short_version.txt")" = "$(RELEASE_VERSION)" || (echo "Bundle short version mismatch"; exit 1)
	@/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_version.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_version.txt")" = "$(RELEASE_VERSION)" || (echo "Bundle version mismatch"; exit 1)
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
	@! find "$(PACKAGE_RESOURCES_DIR)/data/runtime" -type f -print -quit | /usr/bin/grep -q . || (echo "Packaged runtime defaults contain repo-local files"; exit 1)
	@"$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)" --self-test > "$(RELEASE_DIR)/launcher_self_test.txt"
	@! rg -q '/Contents/Resources|args=' "$(RELEASE_DIR)/launcher_self_test.txt" || (echo "Launcher self-test leaked packaged-resource paths or arguments"; exit 1)
	@echo "release-bundle-audit passed."

release-sign: release-bundle-audit
	@test -n "$(RELEASE_CODESIGN_IDENTITY)" || (echo "Missing signing identity"; exit 1)
	@echo "Signing with identity: $(RELEASE_CODESIGN_IDENTITY)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
			[ -f "$$dylib" ] || continue; \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/$(APP_BIN)"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"; \
	else \
		for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
			[ -f "$$dylib" ] || continue; \
			codesign --force --timestamp --options runtime --sign "$(RELEASE_CODESIGN_IDENTITY)" "$$dylib"; \
		done; \
		codesign --force --timestamp --options runtime --sign "$(RELEASE_CODESIGN_IDENTITY)" "$(PACKAGE_MACOS_DIR)/$(APP_BIN)"; \
		codesign --force --timestamp --options runtime --sign "$(RELEASE_CODESIGN_IDENTITY)" "$(PACKAGE_MACOS_DIR)/$(LAUNCHER_BIN)"; \
		codesign --force --timestamp --options runtime --sign "$(RELEASE_CODESIGN_IDENTITY)" "$(PACKAGE_APP_DIR)"; \
	fi
	@echo "release-sign complete."

release-verify: release-sign
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-verify note: ad-hoc identity in use; skipping spctl Gatekeeper assessment"; \
	else \
		spctl_output="$$(spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)" 2>&1)"; \
		spctl_status=$$?; \
		if [ $$spctl_status -ne 0 ]; then \
			if printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "internal error in Code Signing subsystem"; then \
				echo "release-verify note: spctl internal subsystem error on this host; codesign verification remains authoritative"; \
			elif printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "Unnotarized Developer ID"; then \
				echo "release-verify note: app is Developer ID signed but not notarized yet"; \
			else \
				printf '%s\n' "$$spctl_output"; \
				exit $$spctl_status; \
			fi; \
		else \
			printf '%s\n' "$$spctl_output"; \
		fi; \
	fi
	@echo "release-verify passed."

release-verify-signed: release-verify
	@echo "release-verify-signed passed."

release-notarize: release-verify-signed
	@if [ -z "$(APPLE_NOTARY_PROFILE)" ]; then \
		echo "APPLE_NOTARY_PROFILE is required for release-notarize"; \
		exit 1; \
	fi
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-notarize requires a real Developer ID signing identity (APPLE_SIGN_IDENTITY)"; \
		exit 1; \
	fi
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@submission_json="$$(xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json)"; \
	echo "$$submission_json" > "$(RELEASE_DIR)/notary_submit.json"; \
	status="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"status\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/tail -n 1)"; \
	if [ "$$status" != "Accepted" ]; then \
		submission_id="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"id\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/head -n 1)"; \
		echo "release-notarize failed: status=$$status id=$$submission_id"; \
		if [ -n "$$submission_id" ]; then \
			xcrun notarytool log "$$submission_id" --keychain-profile "$(APPLE_NOTARY_PROFILE)" > "$(RELEASE_DIR)/notary_log_$$submission_id.json" || true; \
			echo "notary log: $(RELEASE_DIR)/notary_log_$$submission_id.json"; \
		fi; \
		exit 1; \
	fi
	@echo "release-notarize passed."

release-staple: release-notarize
	@attempt=1; \
	while [ $$attempt -le "$(STAPLE_MAX_ATTEMPTS)" ]; do \
		if xcrun stapler staple "$(PACKAGE_APP_DIR)"; then \
			break; \
		fi; \
		if [ $$attempt -eq "$(STAPLE_MAX_ATTEMPTS)" ]; then \
			echo "release-staple failed after $$attempt attempts"; \
			exit 1; \
		fi; \
		echo "release-staple retry $$attempt/$(STAPLE_MAX_ATTEMPTS) in $(STAPLE_RETRY_DELAY_SEC)s"; \
		sleep "$(STAPLE_RETRY_DELAY_SEC)"; \
		attempt=$$((attempt + 1)); \
	done
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-staple passed."

release-verify-notarized: release-staple
	@spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)"
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-verify-notarized passed."

release-artifact:
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		$(MAKE) BUILD_TOOLCHAIN="$(BUILD_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" release-verify-signed; \
	else \
		$(MAKE) BUILD_TOOLCHAIN="$(BUILD_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" release-verify-notarized; \
	fi
	@mkdir -p "$(RELEASE_DIR)"
	@rm -f "$(RELEASE_APP_ZIP)" "$(RELEASE_APP_ZIP_SHA256)" "$(RELEASE_MANIFEST)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
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
		if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
			echo "signed=ad-hoc"; \
			echo "notarized=0"; \
		else \
			echo "signed=developer-id"; \
			echo "notarized=1"; \
		fi; \
		echo "zip=$(RELEASE_APP_ZIP)"; \
		echo "sha256=$$(cut -d' ' -f1 "$(RELEASE_APP_ZIP_SHA256)")"; \
		if [ "$(RELEASE_CODESIGN_IDENTITY)" != "-" ]; then \
			echo "notary_json=$(RELEASE_DIR)/notary_submit.json"; \
		fi; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

# Fixture/local-adapter proof only. It deliberately accepts only a safe,
# job-scoped relative root and never invokes the distribution target.
release-output-root-contract:
	@case "$(RELEASE_ROOT)" in build/release-authenticated/*) ;; *) echo "RELEASE_ROOT must be a job-scoped build/release-authenticated path"; exit 1;; esac
	@case "$(RELEASE_ROOT)" in */../*|../*|*/..|build/release-authenticated/|*/./*|./*) echo "RELEASE_ROOT must not contain traversal or dot segments"; exit 1;; esac
	@case "$(RELEASE_ROOT)" in *'//'*) echo "RELEASE_ROOT must not contain empty path segments"; exit 1;; esac

release-output-root-conformance: release-output-root-contract
	@test ! -e "$(RELEASE_ROOT)" || (echo "RELEASE_ROOT must be absent before package"; exit 1)
	@DATALAB_RUNTIME_DIR="$(abspath $(RELEASE_DIR)/runtime)" DATALAB_INPUT_ROOT="$(abspath $(RELEASE_DIR)/runtime)" $(MAKE) RELEASE_ROOT="$(RELEASE_ROOT)" release-artifact
	@test -f "$(RELEASE_APP_ZIP)" || (echo "Missing release artifact at selected root"; exit 1)
	@test -f "$(RELEASE_APP_ZIP_SHA256)" || (echo "Missing release checksum at selected root"; exit 1)
	@test -f "$(RELEASE_MANIFEST)" || (echo "Missing release manifest at selected root"; exit 1)
	@test "$(dir $(RELEASE_APP_ZIP))" = "$(RELEASE_ROOT)/" || (echo "Release artifact escaped selected root"; exit 1)
	@echo "release-output-root-conformance passed: $(RELEASE_ROOT)"

release-distribute: release-artifact
	@echo "release-distribute passed."

release-desktop-refresh: release-distribute
	@"$(PACKAGE_APP_RM_GUARD)" "$(DESKTOP_APP_DIR)" "$(PACKAGE_APP_NAME)" desktop
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@spctl --assess --type execute --verbose=2 "$(DESKTOP_APP_DIR)"
	@echo "release-desktop-refresh passed."
