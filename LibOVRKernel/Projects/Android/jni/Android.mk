LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libLibOVR.a
#
# LibOVRKernel
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := libovrkernel		# generate libovrkernel.a

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_ARM_NEON  := true			# compile with neon support enabled
endif

include $(LOCAL_PATH)/../../../../cflags.mk

# Don't export any symbols from LibOVRKernel!
LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Include \
					$(LOCAL_PATH)/../../../Src

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := ../../../Src/Kernel/OVR_Alg.cpp \
                    ../../../Src/Kernel/OVR_Allocator.cpp \
                    ../../../Src/Kernel/OVR_Atomic.cpp \
                    ../../../Src/Kernel/OVR_File.cpp \
                    ../../../Src/Kernel/OVR_FileFILE.cpp \
                    ../../../Src/Kernel/OVR_Log.cpp \
                    ../../../Src/Kernel/OVR_Lockless.cpp \
                    ../../../Src/Kernel/OVR_RefCount.cpp \
                    ../../../Src/Kernel/OVR_Std.cpp \
                    ../../../Src/Kernel/OVR_String.cpp \
                    ../../../Src/Kernel/OVR_String_FormatUtil.cpp \
                    ../../../Src/Kernel/OVR_String_PathUtil.cpp \
                    ../../../Src/Kernel/OVR_SysFile.cpp \
                    ../../../Src/Kernel/OVR_System.cpp \
                    ../../../Src/Kernel/OVR_ThreadsPthread.cpp \
                    ../../../Src/Kernel/OVR_UTF8Util.cpp \
                    ../../../Src/Kernel/OVR_JSON.cpp \
                    ../../../Src/Kernel/OVR_BinaryFile.cpp \
                    ../../../Src/Kernel/OVR_MappedFile.cpp \
                    ../../../Src/Kernel/OVR_MemBuffer.cpp \
                    ../../../Src/Kernel/OVR_Lexer.cpp \
                    ../../../Src/Kernel/OVR_LogUtils.cpp \
                    ../../../Src/Android/JniUtils.cpp \
                    ../../../Src/Kernel/OVR_Signal.cpp

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS
