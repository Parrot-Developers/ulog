LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ulogger
LOCAL_CATEGORY_PATH := utils
LOCAL_DESCRIPTION := A shell command interface to ulog
LOCAL_SRC_FILES := ulogger.c
LOCAL_LIBRARIES := libulog
include $(BUILD_EXECUTABLE)
