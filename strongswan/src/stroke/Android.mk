LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# copy-n-paste from Makefile.am
stroke_SOURCES := \
stroke.c stroke_msg.h stroke_keywords.c stroke_keywords.h

LOCAL_SRC_FILES := $(filter %.c,$(stroke_SOURCES))

# build stroke -----------------------------------------------------------------

LOCAL_C_INCLUDES += \
	$(strongswan_PATH)/src/libstrongswan

LOCAL_CFLAGS := $(strongswan_CFLAGS)

LOCAL_MODULE := stroke
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES += libstrongswan

LOCAL_ADDITIONAL_DEPENDENCIES += $(strongswan_PATH)/Android.mk

include $(MTK_EXECUTABLE)

