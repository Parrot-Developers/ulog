LOCAL_PATH := $(call my-dir)

##############################################################################
# libulog-shd-headers
##############################################################################
include $(CLEAR_VARS)

LOCAL_MODULE := libulog-shd-headers
LOCAL_CATEGORY_PATH := ulog
LOCAL_DESCRIPTION := shared memory blob description for ulog messages

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/include

include $(BUILD_CUSTOM)

##############################################################################
# libulog-shd
##############################################################################
include $(CLEAR_VARS)

LOCAL_MODULE := libulog-shd
LOCAL_CATEGORY_PATH := ulog
LOCAL_DESCRIPTION := shared memory redirection of ulog messages

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

LOCAL_SRC_FILES := \
	src/ulog.c

LOCAL_LIBRARIES := \
	libfutils \
	libshdata \
	libulog

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/include

include $(BUILD_LIBRARY)
