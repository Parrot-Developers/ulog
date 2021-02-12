
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libulog-glib
LOCAL_DESCRIPTION := Redirect glib logging to ulog
LOCAL_CATEGORY_PATH := libs

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := ulog_glib.c

LOCAL_PRIVATE_LIBRARIES := libulog glib

include $(BUILD_STATIC_LIBRARY)

ifneq ("$(TARGET_OS)","darwin")

include $(CLEAR_VARS)
LOCAL_MODULE := libulog-gst
LOCAL_DESCRIPTION := Redirect gstreamer logging to ulog
LOCAL_CATEGORY_PATH := libs

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := ulog_gst.c

LOCAL_PRIVATE_LIBRARIES := libulog gstreamer

include $(BUILD_STATIC_LIBRARY)

endif

include $(CLEAR_VARS)
LOCAL_MODULE := libulog-obus
LOCAL_DESCRIPTION := Redirect obus logging to ulog
LOCAL_CATEGORY_PATH := libs

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := ulog_obus.c

LOCAL_PRIVATE_LIBRARIES := libulog libobus

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libulog-stdcerr
LOCAL_DESCRIPTION := Redirect std cerr logging to ulog
LOCAL_CATEGORY_PATH := libs

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := ulog_stdcerr.cpp

LOCAL_PRIVATE_LIBRARIES := libulog

include $(BUILD_STATIC_LIBRARY)
