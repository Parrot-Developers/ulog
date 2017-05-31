LOCAL_PATH := $(call my-dir)

ifeq ("$(TARGET_OS)","linux")

# libulogcat
include $(CLEAR_VARS)
LOCAL_MODULE := libulogcat
LOCAL_DESCRIPTION := A reader library for ulogger/logger/kernel buffers
LOCAL_CATEGORY_PATH := libs
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wextra -fvisibility=hidden
LOCAL_SRC_FILES := \
	libulogcat_alog.c \
	libulogcat_binary.c \
	libulogcat_ckcm.c \
	libulogcat_core.c \
	libulogcat_klog.c \
	libulogcat_text.c \
	libulogcat_ulog.c

LOCAL_LIBRARIES := libulog
include $(BUILD_SHARED_LIBRARY)

# ulogcat
include $(CLEAR_VARS)
LOCAL_MODULE := ulogcat
LOCAL_CATEGORY_PATH := utils
LOCAL_DESCRIPTION := The equivalent of Android 'logcat' for libulog (and more)
LOCAL_SRC_FILES := \
	ulogcat.c \
	ulogcat_control.c \
	ulogcat_mount.c \
	ulogcat_options.c \
	ulogcat_persist.c \
	ulogcat_pomp.c \
	ulogcat_property.c \
	ulogcat_server.c
LOCAL_LIBRARIES := libulogcat libpomp
LOCAL_CONDITIONAL_LIBRARIES := OPTIONAL:libputils
include $(BUILD_EXECUTABLE)

# ulogcat as a service
include $(CLEAR_VARS)
LOCAL_MODULE := ulogcatd
LOCAL_CATEGORY_PATH := utils
LOCAL_DESCRIPTION := A service running ulogcat to persist/serve log messages
LOCAL_REQUIRED_MODULES := ulogcat
LOCAL_COPY_FILES:= \
	scripts/90-ulogcatd.rc:etc/boxinit.d/ \
	etc/ulogcatd.conf:etc/
include $(BUILD_CUSTOM)

# tests
ifdef TARGET_TEST
include $(CLEAR_VARS)
LOCAL_MODULE := tst-libulogcat
LOCAL_SRC_FILES := tests/libulogcat_test.c
LOCAL_LIBRARIES := libulogcat libulog
include $(BUILD_EXECUTABLE)
endif

endif
