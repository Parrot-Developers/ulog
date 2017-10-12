LOCAL_PATH := $(call my-dir)

ifeq ("$(TARGET_OS)","linux")

# libulogcat
include $(CLEAR_VARS)
LOCAL_MODULE := libulogcat
LOCAL_DESCRIPTION := A reader library for ulogger/kernel buffers
LOCAL_CATEGORY_PATH := libs
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wextra -fvisibility=hidden
LOCAL_SRC_FILES := \
	libulogcat_core.c \
	libulogcat_klog.c \
	libulogcat_text.c \
	libulogcat_compat.c \
	libulogcat_ulog.c

LOCAL_LIBRARIES := libulog
include $(BUILD_SHARED_LIBRARY)

# ulogcat
include $(CLEAR_VARS)
LOCAL_MODULE := ulogcat
LOCAL_CATEGORY_PATH := utils
LOCAL_DESCRIPTION := The equivalent of Android 'logcat' for libulog (and more)
LOCAL_SRC_FILES := ulogcat.c
LOCAL_LIBRARIES := libulogcat
include $(BUILD_EXECUTABLE)

# tests
ifdef TARGET_TEST
include $(CLEAR_VARS)
LOCAL_MODULE := tst-libulogcat
LOCAL_SRC_FILES := tests/libulogcat_test.c
LOCAL_LIBRARIES := libulogcat libulog
include $(BUILD_EXECUTABLE)
endif

endif
