LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libulogcat
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wextra -fvisibility=hidden
LOCAL_SRC_FILES := \
	libulogcat_alog.c \
	libulogcat_binary.c \
	libulogcat_ckcm.c \
	libulogcat_core.c \
	libulogcat_klog.c \
	libulogcat_text.c \
	libulogcat_ulog.c
LOCAL_SHARED_LIBRARIES := libulog
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := ulogcat
LOCAL_SRC_FILES := \
	ulogcat.c \
	ulogcat_control.c \
	ulogcat_mount.c \
	ulogcat_options.c \
	ulogcat_persist.c \
	ulogcat_pomp.c \
	ulogcat_property.c \
	ulogcat_server.c

LOCAL_SHARED_LIBRARIES := libulogcat libpomp libcutils
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
