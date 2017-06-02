LIBULOG_DIR := $(THREADX_SRC_DIR)/$(src)

libulog-objs += src/ulog.o

ifeq ($(CONFIG_PARROT_LINUXLOG),y)
ccflags-y += -I$(abspath $(LIBULOG_DIR))/ulog_shd/include
libulog-objs += src/ulog_ctrl.o
endif

obj-y += libulog.a
install-files += libulog.a
