# This file is included in all .mk files to ensure their compilation flags are in sync
# across debug and release builds.

LOCAL_CFLAGS	:= -DANDROID_NDK
LOCAL_CFLAGS	+= -Werror			# error on warnings
LOCAL_CFLAGS	+= -Wall
LOCAL_CFLAGS	+= -Wextra
#LOCAL_CFLAGS	+= -Wlogical-op		# not part of -Wall or -Wextra
#LOCAL_CFLAGS	+= -Weffc++			# too many issues to fix for now
LOCAL_CFLAGS	+= -Wno-strict-aliasing		# TODO: need to rewrite some code
LOCAL_CFLAGS	+= -Wno-unused-parameter
LOCAL_CFLAGS	+= -Wno-missing-field-initializers	# warns on this: SwipeAction	ret = {}
LOCAL_CFLAGS	+= -Wno-multichar	# used in internal Android headers:  DISPLAY_EVENT_VSYNC = 'vsyn',
LOCAL_CPPFLAGS += -Wno-invalid-offsetof
LOCAL_CPPFLAGS += -std=c++11

ifeq ($(OVR_DEBUG),1)
  LOCAL_CFLAGS += -DOVR_BUILD_DEBUG=1 -O0 -g
else
  LOCAL_CFLAGS += -O3
endif

# Explicitly compile for the ARM and not the Thumb instruction set.
LOCAL_ARM_MODE := arm
