.DEFAULT_GOAL := all

include make/config.mk
include make/target.mk
include make/paths.mk
include make/shared.mk
include make/flags.mk
include make/sources.mk

.PHONY: all clean test test-smoke test-pack-loader test-contract test-app-contract \
	test-authoring-input-contract test-raster-viewport-contract test-loop-policy-contract \
	test-panel-policy-contract test-profile-interaction-contract test-datalab-folder-picker test-image-residency-contract test-raster-generation-contract test-async-decode-contract test-thumbnail-decode-contract test-linux-launcher-contract test-package-boundary \
	test-input-catalog-contract test-focus-window-contract \
	test-w5-acceptance \
	run run-headless run-headless-smoke visual-harness visual-artifact test-stable test-legacy \
	vulkan-rollout-contract vulkan-rollout-self-test package-desktop-vulkan-self-test \
	memory-check-build memory-check-run memory-check-audit \
	package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop \
	package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh \
	test-package-desktop-path-guard test-package-runtime-boundary \
	package-linux-desktop package-linux-desktop-clean package-linux-desktop-host-check \
	package-linux-desktop-contract package-linux-desktop-self-test package-linux-desktop-determinism-test \
	release-contract release-clean release-build release-bundle-audit release-sign release-verify \
	release-verify-signed release-notarize release-staple release-verify-notarized release-artifact \
	release-distribute release-desktop-refresh release-output-root-contract \
	release-output-root-conformance

include make/rules-build.mk
include make/rules-test.mk
include make/rules-memory-check.mk
include make/package-macos.mk
include make/package-linux-desktop.mk
include make/release.mk
