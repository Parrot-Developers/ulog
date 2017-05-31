LOCAL_PATH := $(call my-dir)

ifeq ("$(TARGET_OS)","linux")

include $(CLEAR_VARS)
LOCAL_MODULE := libulog_syslogwrap
LOCAL_DESCRIPTION := A small wrapper library for redirecting syslog to ulog
LOCAL_CATEGORY_PATH := libs
LOCAL_CFLAGS := -Wextra
LOCAL_SRC_FILES := ulog_syslog.c
LOCAL_LIBRARIES := libulog

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := ulogwrapper
LOCAL_CATEGORY_PATH := utils
LOCAL_DESCRIPTION := A small executable wrapper for redirecting syslog to ulog
LOCAL_SRC_FILES := ulogwrapper.c
LOCAL_DEPENDS_HEADERS := libulog
LOCAL_REQUIRED_MODULES := libulog_syslogwrap

include $(BUILD_EXECUTABLE)

endif
