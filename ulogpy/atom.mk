LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libulog-py
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Python ulog logging integration
LOCAL_DEPENDS_MODULES := python
LOCAL_LIBRARIES := libulog

# python ulog logging module
PRIVATE_PYTHON_ULOG_ROOT_DIR = \
	$(TARGET_OUT_STAGING)$(shell echo $${TARGET_DEPLOY_ROOT:-/usr})
PRIVATE_PYTHON_ULOG_OUT_DIR = \
	$(PRIVATE_PYTHON_ULOG_ROOT_DIR)/lib/python/site-packages
LOCAL_COPY_FILES := ulog.py:$(PRIVATE_PYTHON_ULOG_OUT_DIR)/ulog.py

# python ulog binding generation
LOCAL_EXPAND_CUSTOM_VARIABLES := 1
ULOGPY_LIBS_DIR := \
	$(TARGET_OUT_STAGING)$(shell echo $${TARGET_DEPLOY_ROOT:-/usr})/lib/
LOCAL_CUSTOM_MACROS := \
	pybinding-macro:_ulog,$\
	libulog,$\
	LIBULOG_HEADERS,$\
	$(ULOGPY_LIBS_DIR)libulog$(TARGET_SHARED_LIB_SUFFIX)

include $(BUILD_CUSTOM)
