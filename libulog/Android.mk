LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libulog
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wextra -fvisibility=hidden
LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_SRC_FILES := ulog_write.c ulog_read.c ulog_write_android.c
LOCAL_MODULE_TAGS := optional

ifdef NDK_PROJECT_PATH
  LOCAL_CFLAGS += -DANDROID_NDK
  LOCAL_EXPORT_C_INCLUDES += $(LOCAL_EXPORT_C_INCLUDE_DIRS)
  include $(BUILD_STATIC_LIBRARY)
else
  LOCAL_SHARED_LIBRARIES += liblog
  include $(BUILD_SHARED_LIBRARY)
endif
