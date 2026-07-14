LINUX_DESKTOP_PLATFORM ?= linux-x86_64
LINUX_DESKTOP_PACKAGE_EPOCH ?= 0
LINUX_DESKTOP_PACKAGE_CLASS := desktop_app_linux
LINUX_DESKTOP_ARTIFACT_ROLE := desktop_app
LINUX_DESKTOP_RUNTIME := linux_gui
LINUX_DESKTOP_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(LINUX_DESKTOP_PLATFORM)-desktop-$(RELEASE_CHANNEL)
LINUX_DESKTOP_DIR := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME)
LINUX_DESKTOP_BIN_DIR := $(LINUX_DESKTOP_DIR)/bin
LINUX_DESKTOP_RESOURCES_DIR := $(LINUX_DESKTOP_DIR)/resources
LINUX_DESKTOP_SHARE_DIR := $(LINUX_DESKTOP_DIR)/share
LINUX_DESKTOP_LAUNCHER_SRC := tools/packaging/linux/datalab-launcher
LINUX_DESKTOP_ENTRY_SRC := tools/packaging/linux/scope.desktop
LINUX_DESKTOP_ICON_SRC := tools/packaging/linux/icons/scope.svg
LINUX_DESKTOP_INSTALLER_SRC := tools/packaging/linux/install-desktop-entry.sh
LINUX_DESKTOP_ENTRY := $(LINUX_DESKTOP_SHARE_DIR)/applications/scope.desktop
LINUX_DESKTOP_ICON := $(LINUX_DESKTOP_SHARE_DIR)/icons/hicolor/scalable/apps/scope.svg
LINUX_DESKTOP_INSTALLER := $(LINUX_DESKTOP_SHARE_DIR)/install-desktop-entry.sh
LINUX_DESKTOP_MANIFEST_JSON := $(LINUX_DESKTOP_DIR)/manifest.json
LINUX_DESKTOP_PACKAGE_MANIFEST := $(LINUX_DESKTOP_DIR)/package_manifest.json
LINUX_DESKTOP_ARCHIVE := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME).tar.gz
LINUX_DESKTOP_SHA256 := $(LINUX_DESKTOP_ARCHIVE).sha256
LINUX_DESKTOP_SELF_TEST_ROOT := $(RELEASE_DIR)/linux-desktop-self-test
LINUX_DESKTOP_SELF_TEST_HOME := $(abspath $(LINUX_DESKTOP_SELF_TEST_ROOT)/home)
LINUX_DESKTOP_SELF_TEST_DATA := $(abspath $(LINUX_DESKTOP_SELF_TEST_ROOT)/xdg-data)
LINUX_DESKTOP_SELF_TEST_STATE := $(abspath $(LINUX_DESKTOP_SELF_TEST_ROOT)/xdg-state)
LINUX_DESKTOP_SELF_TEST_CONFIG := $(abspath $(LINUX_DESKTOP_SELF_TEST_ROOT)/xdg-config)

package-linux-desktop-contract:

	@echo "package_class=$(LINUX_DESKTOP_PACKAGE_CLASS)"
	@echo "artifact_role=$(LINUX_DESKTOP_ARTIFACT_ROLE)"
	@echo "runtime=$(LINUX_DESKTOP_RUNTIME)"
	@echo "program=$(RELEASE_PROGRAM_KEY)"
	@echo "product=$(RELEASE_PRODUCT_NAME)"
	@echo "version=$(RELEASE_VERSION)"
	@echo "platform=$(LINUX_DESKTOP_PLATFORM)"
	@echo "archive=$(LINUX_DESKTOP_ARCHIVE)"

package-linux-desktop-clean:
	@rm -rf "$(LINUX_DESKTOP_DIR)" "$(LINUX_DESKTOP_ARCHIVE)" "$(LINUX_DESKTOP_SHA256)" "$(LINUX_DESKTOP_SELF_TEST_ROOT)"
	@echo "Removed Linux desktop package artifacts: $(LINUX_DESKTOP_BASENAME)"

package-linux-desktop-host-check:
	@if [ "$$(uname -s)" != "Linux" ]; then \
		echo "package-linux-desktop must run on a Linux host; use the Linux PC handoff lane for proof"; \
		exit 2; \
	fi
	@if [ "$(LINUX_DESKTOP_PLATFORM)" != "linux-x86_64" ]; then \
		echo "unsupported Linux desktop platform: $(LINUX_DESKTOP_PLATFORM)"; \
		exit 2; \
	fi

package-linux-desktop: package-linux-desktop-host-check $(PACKAGE_SOURCE_BIN)
	@echo "Preparing Linux desktop package..."
	@rm -rf "$(LINUX_DESKTOP_DIR)" "$(LINUX_DESKTOP_ARCHIVE)" "$(LINUX_DESKTOP_SHA256)"
	@mkdir -p "$(LINUX_DESKTOP_BIN_DIR)" "$(LINUX_DESKTOP_RESOURCES_DIR)/data/runtime" \
		"$(LINUX_DESKTOP_RESOURCES_DIR)/data/import" "$(dir $(LINUX_DESKTOP_ENTRY))" "$(dir $(LINUX_DESKTOP_ICON))"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(LINUX_DESKTOP_BIN_DIR)/datalab-bin"
	@cp "$(LINUX_DESKTOP_LAUNCHER_SRC)" "$(LINUX_DESKTOP_BIN_DIR)/datalab-launcher"
	@cp "$(LINUX_DESKTOP_ENTRY_SRC)" "$(LINUX_DESKTOP_ENTRY)"
	@cp "$(LINUX_DESKTOP_ICON_SRC)" "$(LINUX_DESKTOP_ICON)"
	@cp "$(LINUX_DESKTOP_INSTALLER_SRC)" "$(LINUX_DESKTOP_INSTALLER)"
	@chmod +x "$(LINUX_DESKTOP_BIN_DIR)/datalab-bin" "$(LINUX_DESKTOP_BIN_DIR)/datalab-launcher" "$(LINUX_DESKTOP_INSTALLER)"
	@mkdir -p "$(LINUX_DESKTOP_RESOURCES_DIR)/shared/assets" "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer" "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders"
	@cp -R "$(SHARED_ROOT)/assets/fonts" "$(LINUX_DESKTOP_RESOURCES_DIR)/shared/assets/"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders/"
	@printf '{\n' > "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "schema_version": "codework-desktop-package/v1",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "package_class": "%s",\n' "$(LINUX_DESKTOP_PACKAGE_CLASS)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "artifact_role": "%s",\n' "$(LINUX_DESKTOP_ARTIFACT_ROLE)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime": "%s",\n' "$(LINUX_DESKTOP_RUNTIME)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "entrypoint": "bin/datalab-launcher",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "desktop_entry": "share/applications/scope.desktop",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "icon": "share/icons/hicolor/scalable/apps/scope.svg",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "desktop_installer": "share/install-desktop-entry.sh",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "libpng", "vulkan-loader", "vulkan-driver"]\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '}\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '{\n' > "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "schema_version": "codework_package_manifest_v1",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_class": "%s",\n' "$(LINUX_DESKTOP_PACKAGE_CLASS)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_role": "%s",\n' "$(LINUX_DESKTOP_ARTIFACT_ROLE)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "runtime": "%s",\n' "$(LINUX_DESKTOP_RUNTIME)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "entrypoints": {"desktop_launcher": "bin/datalab-launcher", "runtime_binary": "bin/datalab-bin"},\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "desktop_integration": {"desktop_entry": "share/applications/scope.desktop", "icon": "share/icons/hicolor/scalable/apps/scope.svg", "installer": "share/install-desktop-entry.sh"},\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "self_test": {"type": "command", "argv": ["bin/datalab-launcher", "--self-test"]}\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '}\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '# sCope Linux desktop package\n\nPrivate proof package for the DataLab visual-output workspace. Run `bin/datalab-launcher --self-test` after unpacking.\n' > "$(LINUX_DESKTOP_DIR)/README.md"
	@cd "$(RELEASE_DIR)" && find "$(LINUX_DESKTOP_BASENAME)" -print0 | LC_ALL=C sort -z | tar --null --no-recursion --files-from - --format=posix --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime --mtime="@$(LINUX_DESKTOP_PACKAGE_EPOCH)" --owner=0 --group=0 --numeric-owner -cf - | gzip -n > "$(abspath $(LINUX_DESKTOP_ARCHIVE))"
	@cd "$(RELEASE_DIR)" && sha256sum "$(notdir $(LINUX_DESKTOP_ARCHIVE))" > "$(notdir $(LINUX_DESKTOP_SHA256))"
	@echo "Linux desktop package ready: $(LINUX_DESKTOP_ARCHIVE)"

package-linux-desktop-self-test: package-linux-desktop
	@$(MAKE) test-datalab-folder-picker
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/datalab-launcher" || (echo "Missing Linux launcher"; exit 1)
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/datalab-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(LINUX_DESKTOP_ENTRY)" || (echo "Missing Linux desktop entry"; exit 1)
	@test -f "$(LINUX_DESKTOP_ICON)" || (echo "Missing Linux desktop icon"; exit 1)
	@test -x "$(LINUX_DESKTOP_INSTALLER)" || (echo "Missing Linux desktop installer"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/shared/assets/fonts/Lato-Regular.ttf" || (echo "Missing bundled font"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk shader"; exit 1)
	@test -f "$(LINUX_DESKTOP_ARCHIVE)" || (echo "Missing Linux desktop archive"; exit 1)
	@test -f "$(LINUX_DESKTOP_SHA256)" || (echo "Missing Linux desktop checksum"; exit 1)
	@rm -rf "$(LINUX_DESKTOP_SELF_TEST_ROOT)"
	@mkdir -p "$(LINUX_DESKTOP_SELF_TEST_ROOT)/unpack" "$(LINUX_DESKTOP_SELF_TEST_HOME)" "$(LINUX_DESKTOP_SELF_TEST_DATA)" "$(LINUX_DESKTOP_SELF_TEST_STATE)" "$(LINUX_DESKTOP_SELF_TEST_CONFIG)"
	@cd "$(RELEASE_DIR)" && sha256sum -c "$(notdir $(LINUX_DESKTOP_SHA256))"
	@tar -xzf "$(LINUX_DESKTOP_ARCHIVE)" -C "$(LINUX_DESKTOP_SELF_TEST_ROOT)/unpack"
	@HOME="$(LINUX_DESKTOP_SELF_TEST_HOME)" XDG_DATA_HOME="$(LINUX_DESKTOP_SELF_TEST_DATA)" "$(LINUX_DESKTOP_SELF_TEST_ROOT)/unpack/$(LINUX_DESKTOP_BASENAME)/share/install-desktop-entry.sh" >/dev/null
	@test -f "$(LINUX_DESKTOP_SELF_TEST_DATA)/applications/scope.desktop" || (echo "Installer did not write scope.desktop"; exit 1)
	@test -f "$(LINUX_DESKTOP_SELF_TEST_DATA)/icons/hicolor/scalable/apps/scope.svg" || (echo "Installer did not write scope.svg"; exit 1)
	@HOME="$(LINUX_DESKTOP_SELF_TEST_HOME)" XDG_DATA_HOME="$(LINUX_DESKTOP_SELF_TEST_DATA)" XDG_STATE_HOME="$(LINUX_DESKTOP_SELF_TEST_STATE)" XDG_CONFIG_HOME="$(LINUX_DESKTOP_SELF_TEST_CONFIG)" "$(LINUX_DESKTOP_SELF_TEST_ROOT)/unpack/$(LINUX_DESKTOP_BASENAME)/bin/datalab-launcher" --self-test
	@echo "package-linux-desktop-self-test passed."

package-linux-desktop-determinism-test: package-linux-desktop-self-test
	@release_dir="$(abspath $(RELEASE_DIR))"; archive_path="$(abspath $(LINUX_DESKTOP_ARCHIVE))"; sha_path="$(abspath $(LINUX_DESKTOP_SHA256))"; first_sha="$$(cut -d ' ' -f 1 "$$sha_path")"; rm -f "$$archive_path" "$$sha_path"; cd "$$release_dir" && find "$(LINUX_DESKTOP_BASENAME)" -print0 | LC_ALL=C sort -z | tar --null --no-recursion --files-from - --format=posix --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime --mtime="@$(LINUX_DESKTOP_PACKAGE_EPOCH)" --owner=0 --group=0 --numeric-owner -cf - | gzip -n > "$$archive_path"; cd "$$release_dir" && sha256sum "$(notdir $(LINUX_DESKTOP_ARCHIVE))" > "$(notdir $(LINUX_DESKTOP_SHA256))"; second_sha="$$(cut -d ' ' -f 1 "$$sha_path")"; test "$$first_sha" = "$$second_sha" || { echo "package-linux-desktop determinism failed"; exit 1; }; echo "package-linux-desktop-determinism-test passed: $$second_sha"
