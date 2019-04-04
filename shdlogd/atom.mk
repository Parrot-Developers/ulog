LOCAL_PATH := $(call my-dir)

##############################################################################
# shdlogd
##############################################################################
include $(CLEAR_VARS)

LOCAL_MODULE := shdlogd
LOCAL_CATEGORY_PATH := ulog
LOCAL_DESCRIPTION := daemon reading ulogs in the shared memory

LOCAL_SRC_FILES := \
	src/shdlogd.c

LOCAL_LIBRARIES := \
	libshdata \
	libpomp \
	libulog-shd-headers \
	libulog \
	libfutils

LOCAL_COPY_FILES := \
	etc/boxinit.d/10-shdlogd.rc:etc/boxinit.d/

include $(BUILD_EXECUTABLE)
