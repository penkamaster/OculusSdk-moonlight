# MAKEFILE_LIST specifies the current used Makefiles, of which this is the last
# one. I use that to obtain the Application.mk dir then import the root
# Application.mk.
ROOT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))../../../../
include $(ROOT_DIR)/Application.mk
APP_MODULES := vrappframework

# SDK Libs are ALWAYS built with 64-bit support!
APP_ABI := armeabi-v7a,arm64-v8a