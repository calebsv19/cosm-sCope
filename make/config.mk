PROGRAM_KEY := datalab
TARGET_NAME := datalab
APP_BIN := datalab-bin
LAUNCHER_BIN := datalab-launcher
PACKAGE_APP_NAME := sCope.app
RELEASE_PRODUCT_NAME := sCope
RELEASE_BUNDLE_ID := com.cosm.scope

HOST_CC ?= cc
FISICS_CC ?= /Users/calebsv/Desktop/CodeWork/fisiCs/fisics
BUILD_TOOLCHAIN ?= clang
PACKAGE_TOOLCHAIN ?= $(BUILD_TOOLCHAIN)
RELEASE_TOOLCHAIN := clang
PKG_CONFIG ?= pkg-config
CSTD ?= -std=c11
WARN ?= -Wall -Wextra -Wpedantic
DEBUG ?= -g
RELEASE_CHANNEL ?= stable
VERSION_FILE ?= VERSION
TARGET_CONTRACT_HELPER ?= ../bin/desktop_release_target_contract.sh
