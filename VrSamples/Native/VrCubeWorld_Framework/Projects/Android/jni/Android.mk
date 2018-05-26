LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrcubeworld.so
#--------------------------------------------------------
include $(CLEAR_VARS)

include ../../../../../cflags.mk

LOCAL_MODULE			:= vrcubeworld
LOCAL_SRC_FILES			:= ../../../Src/VrCubeWorld_Framework.cpp
LOCAL_STATIC_LIBRARIES	+= vrsound vrlocale vrgui vrappframework libovrkernel
LOCAL_SHARED_LIBRARIES	+= vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,LibOVRKernel/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrAppFramework/Projects/Android/jni)
$(call import-module,VrAppSupport/VrGUI/Projects/Android/jni)
$(call import-module,VrAppSupport/VrLocale/Projects/Android/jni)
$(call import-module,VrAppSupport/VrSound/Projects/Android/jni)
