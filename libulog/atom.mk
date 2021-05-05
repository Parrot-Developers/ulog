LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libulog
LOCAL_DESCRIPTION := A minimalistic logging library derived from Android logger
LOCAL_CATEGORY_PATH := libs
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_CUSTOM_VARIABLES := LIBULOG_HEADERS=$\
	$(LOCAL_PATH)/include/ulog.h:$(LOCAL_PATH)/include/ulograw.h;
LOCAL_CFLAGS := -fvisibility=hidden

LOCAL_SRC_FILES := ulog_read.c ulog_write.c

ifeq ("$(TARGET_OS)","windows")
  LOCAL_SRC_FILES += ulog.cpp
  LOCAL_LDLIBS += -lpthread
else
  LOCAL_SRC_FILES += ulog.cpp ulog_write_android.c ulog_write_bin.c ulog_write_raw.c
endif

ifeq ("$(TARGET_OS)-$(TARGET_OS_FLAVOUR)","linux-android")
ifdef USE_ALCHEMY_ANDROID_SDK
LOCAL_LIBRARIES += liblog libstlport
else
LOCAL_LDLIBS += -llog
endif
endif

include $(BUILD_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libulog-vala

# Install vapi files
LOCAL_INSTALL_HEADERS := \
	libulog.vapi:$(TARGET_OUT_STAGING)/usr/share/vala/vapi/

include $(BUILD_CUSTOM)

include $(CLEAR_VARS)
LOCAL_MODULE := libulog-testc
LOCAL_DESCRIPTION := libulog test of C API
LOCAL_CATEGORY_PATH := test
LOCAL_CFLAGS := -Wno-format-security -Wno-format-nonliteral
LOCAL_SRC_FILES := tests/ulogtest.c

LOCAL_LIBRARIES := libulog

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := libulog-testcpp
LOCAL_DESCRIPTION := libulog test of C++ API
LOCAL_CATEGORY_PATH := test
LOCAL_SRC_FILES := tests/ulogtest.cpp

LOCAL_LIBRARIES := libulog libulog-stdcerr

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := ulog_shell_api
LOCAL_DESCRIPTION := shell functions to ease the usage of ulog's API
LOCAL_CATEGORY_PATH := ulog
LOCAL_COPY_FILES := ulog_api.sh:usr/share/ulog/
include $(BUILD_CUSTOM)
