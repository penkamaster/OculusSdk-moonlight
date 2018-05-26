LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrlocale.a
#
# VrLocale
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := vrlocale			# generate libvrlocale.a

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb
LOCAL_ARM_NEON  := true				# compile with neon support enabled

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Include

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Include

LOCAL_SRC_FILES := 	../../../Src/OVR_Locale.cpp \
					../../../Src/tinyxml2.cpp

LOCAL_STATIC_LIBRARIES := vrappframework
					
include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module,VrAppFramework/Projects/Android/jni)
