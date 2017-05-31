LOCAL_PATH := $(call my-dir)

ifeq ("$(TARGET_OS)","linux")

include $(CLEAR_VARS)
LOCAL_MODULE := kmsgd
LOCAL_CATEGORY_PATH := utils
LOCAL_DESCRIPTION := A daemon logging kernel messages to a ulog buffer
LOCAL_SRC_FILES := kmsgd.c
LOCAL_LIBRARIES := libulog
LOCAL_COPY_FILES:= scripts/90-kmsgd.rc:etc/boxinit.d/
include $(BUILD_EXECUTABLE)

endif
