#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE_MAKE="${ROOT_DIR}/make/package-macos.mk"
CONFIG_MAKE="${ROOT_DIR}/make/config.mk"
PATHS_MAKE="${ROOT_DIR}/make/paths.mk"
LAUNCHER="${ROOT_DIR}/tools/packaging/macos/datalab-launcher"
INFO_PLIST="${ROOT_DIR}/tools/packaging/macos/Info.plist"

fail() { echo "Main Edit package contract check failed: $1" >&2; exit 1; }
check() { rg --fixed-strings --quiet -- "$1" "$2" || fail "missing '$1' in $2"; }

check "package-desktop-main-edit" "${PACKAGE_MAKE}"
check "package-desktop-main-edit-self-test" "${PACKAGE_MAKE}"
check "package-desktop-main-edit-refresh" "${PACKAGE_MAKE}"
check "MAIN_EDIT_APP_NAME := sCope Main Edit.app" "${PATHS_MAKE}"
check "MAIN_EDIT_BUNDLE_ID := com.cosm.scope.main-edit" "${PATHS_MAKE}"
check "MAIN_EDIT_RUNTIME_NAMESPACE := DataLab-Main-Edit" "${PATHS_MAKE}"
check 'MAIN_EDIT_BUILD_LABEL = sCope-main-edit-$(RELEASE_VERSION)' "${PATHS_MAKE}"
check "PACKAGE_PROFILE ?= standard" "${CONFIG_MAKE}"
check "write-identity" "${PACKAGE_MAKE}"
check "verify-identity" "${PACKAGE_MAKE}"
check "Source changed during Main Edit packaging" "${PACKAGE_MAKE}"
check "Refusing canonical Desktop destination" "${PACKAGE_MAKE}"
check "process-audit" "${PACKAGE_MAKE}"
check 'Application Support/${RUNTIME_NAMESPACE}' "${LAUNCHER}"
check 'Library/Logs/${LOG_NAMESPACE}' "${LAUNCHER}"
check "DataLabPackageProfile" "${INFO_PLIST}"

echo "Main Edit package contract checks passed"
