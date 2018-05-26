LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# OpenGL Loader
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := openglloader		# generate libopenglloader.a

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_ARM_NEON  := true			# compile with neon support enabled
endif

include $(LOCAL_PATH)/../../../../../cflags.mk

# Don't export any symbols!
LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Include

# only export public headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := ../../../Src/gles2_loader.cpp \
					../../../Src/gles2ext_loader.cpp \
					../../../Src/gles3_loader.cpp

# GL platform interface
LOCAL_EXPORT_LDLIBS += -lEGL

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS
