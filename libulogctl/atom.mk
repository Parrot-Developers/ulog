LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libulogctl
LOCAL_DESCRIPTION := Ulog controller in run time.
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include/
LOCAL_CFLAGS := -DULOGCTL_API_EXPORTS -fvisibility=hidden
LOCAL_SRC_FILES := \
	src/ulogctl_srv.c \
	src/ulogctl_cli.c \
	src/ulogctl_log.c
LOCAL_LIBRARIES := \
	libpomp \
	libulog
include $(BUILD_LIBRARY)

###############################################################################
###############################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := ulogctl
LOCAL_SRC_FILES := tool/ulogctl.c
LOCAL_LIBRARIES := libulogctl libpomp libulog
include $(BUILD_EXECUTABLE)

###############################################################################
###############################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := ulogctl-srv
LOCAL_SRC_FILES := examples/ulogctl-srv.c
LOCAL_LIBRARIES := libulogctl libpomp libulog
include $(BUILD_EXECUTABLE)
