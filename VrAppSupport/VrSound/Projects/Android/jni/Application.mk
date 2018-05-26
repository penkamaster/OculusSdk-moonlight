# Application.mk for VrSound.
ROOT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))../../../../../
include $(ROOT_DIR)/Application.mk
APP_MODULES := vrsound

# SDK Libs are ALWAYS built with 64-bit support!
APP_ABI := armeabi-v7a,arm64-v8a