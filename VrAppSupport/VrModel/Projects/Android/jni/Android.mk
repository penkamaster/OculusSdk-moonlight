LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrmodel.a
#
# VrModel
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

#APP_MODULE     := vrmodel

LOCAL_MODULE    := vrmodel			# generate libvrmodel.a

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb
LOCAL_ARM_NEON  := true				# compile with neon support enabled

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Src

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Src

LOCAL_SRC_FILES := 	../../../Src/ModelFile.cpp \
					../../../Src/ModelFile_glTF.cpp \
					../../../Src/ModelFile_OvrScene.cpp \
					../../../Src/ModelCollision.cpp \
					../../../Src/ModelTrace.cpp \
					../../../Src/ModelRender.cpp \
					../../../Src/SceneView.cpp

LOCAL_STATIC_LIBRARIES := vrappframework
					
include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module,VrAppFramework/Projects/Android/jni)
