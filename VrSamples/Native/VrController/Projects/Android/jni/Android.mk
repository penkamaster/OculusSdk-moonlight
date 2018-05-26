LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../../../cflags.mk

LOCAL_MODULE			:= vrcontroller
LOCAL_SRC_FILES			:= 	../../../Src/VrController.cpp	\
							../../../Src/EaseFunctions.cpp	\
							../../../Src/TextureAtlas.cpp	\
							../../../Src/ParticleSystem.cpp	\
							../../../Src/BeamRenderer.cpp  	\
							../../../Src/OVR_Skeleton.cpp	\
							../../../Src/OVR_ArmModel.cpp \
							../../../Src/ControllerGUI.cpp \
							../../../Src/PointList.cpp \
							../../../Src/Ribbon.cpp
							

LOCAL_STATIC_LIBRARIES	:= vrsound vrmodel vrlocale vrgui vrappframework libovrkernel
LOCAL_SHARED_LIBRARIES	:= vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,LibOVRKernel/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrAppFramework/Projects/Android/jni)
$(call import-module,VrAppSupport/VrGUI/Projects/Android/jni)
$(call import-module,VrAppSupport/VrLocale/Projects/Android/jni)
$(call import-module,VrAppSupport/VrModel/Projects/Android/jni)
$(call import-module,VrAppSupport/VrSound/Projects/Android/jni)