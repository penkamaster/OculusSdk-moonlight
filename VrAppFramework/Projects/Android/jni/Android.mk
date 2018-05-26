LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrappframework.a
#
# VrAppFramework
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := vrappframework	# generate libvrappframework.a

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb
LOCAL_ARM_NEON  := true				# compile with neon support enabled

include $(LOCAL_PATH)/../../../../cflags.mk

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../../../LibOVRKernel/Src \
  $(LOCAL_PATH)/../../../../VrApi/Include \
  $(LOCAL_PATH)/../../../Include

# We export the includes for LibOVRKernel and VrApi because don't do explicit imports of
# those libraries because they may be pre-built or dynamic. Other projects that
# link to VrAppFramework will get the LibOVRKernel and VrApi include paths this way with
# doing an import.
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := ../../../Src/BitmapFont.cpp \
                    ../../../Src/ImageData.cpp \
                    ../../../Src/GlSetup.cpp \
                    ../../../Src/GlSetup_Android.cpp \
                    ../../../Src/GlTexture.cpp \
                    ../../../Src/GlTexture_Android.cpp \
                    ../../../Src/GlProgram.cpp \
                    ../../../Src/GlGeometry.cpp \
                    ../../../Src/GlBuffer.cpp \
                    ../../../Src/PackageFiles.cpp \
                    ../../../Src/SurfaceTexture.cpp \
                    ../../../Src/VrCommon.cpp \
                    ../../../Src/Framebuffer.cpp \
                    ../../../Src/EyeBuffers.cpp \
                    ../../../Src/MessageQueue.cpp \
                    ../../../Src/TalkToJava.cpp \
                    ../../../Src/KeyState.cpp \
                    ../../../Src/App.cpp \
                    ../../../Src/App_Android.cpp \
                    ../../../Src/AppRender.cpp \
                    ../../../Src/PathUtils.cpp \
                    ../../../Src/SurfaceRender.cpp \
                    ../../../Src/DebugLines.cpp \
                    ../../../Src/VrFrameBuilder.cpp \
                    ../../../Src/Console.cpp \
                    ../../../Src/OVR_GlUtils.cpp \
                    ../../../Src/OVR_Geometry.cpp \
                    ../../../Src/OVR_Input.cpp \
                    ../../../Src/OVR_Uri.cpp \
                    ../../../Src/OVR_FileSys.cpp \
                    ../../../Src/OVR_LogTimer.cpp \
                    ../../../Src/OVR_Stream.cpp \
                    ../../../Src/JobManager.cpp \
                    ../../../Src/OVR_TextureManager.cpp \
                    ../../../Src/SystemClock.cpp

# GL platform interface
LOCAL_EXPORT_LDLIBS += -lEGL
# native multimedia
LOCAL_EXPORT_LDLIBS += -lOpenMAXAL
# logging
LOCAL_EXPORT_LDLIBS += -llog
# native windows
LOCAL_EXPORT_LDLIBS += -landroid
# For minizip
LOCAL_EXPORT_LDLIBS += -lz
# audio
LOCAL_EXPORT_LDLIBS += -lOpenSLES

LOCAL_STATIC_LIBRARIES += libovrkernel minizip stb openglloader
LOCAL_SHARED_LIBRARIES := vrapi

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module,3rdParty/minizip/build/android/jni)
$(call import-module,3rdParty/stb/build/android/jni)
$(call import-module,1stParty/OpenGL_Loader/Projects/Android/jni)

# Note: Even though we depend on LibOVRKernel, we don't explicitly import it since our
# dependents may want either a prebuilt or from-source LibOVRKernel.
