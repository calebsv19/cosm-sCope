include make/config.mk
include make/target.mk
include make/paths.mk
include make/shared.mk
include make/flags.mk
include make/sources.mk

.PHONY: all clean test run run-headless run-headless-smoke visual-harness test-stable test-legacy \
	package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop \
	package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh \
	release-contract release-clean release-build release-bundle-audit release-sign release-verify \
	release-verify-signed release-notarize release-staple release-verify-notarized release-artifact \
	release-distribute release-desktop-refresh

include make/rules-build.mk
include make/rules-test.mk
include make/package-macos.mk
include make/release.mk
