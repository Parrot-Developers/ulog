
subdir-ccflags-y += -I$(parrottree)/packages/ulog/libulog/include
# Parrot libraries have optional dependency to libulog
subdir-ccflags-y += -DBUILD_LIBULOG

libparrot-objs += parrot/ulog/src/ulog.o

ifeq ($(CONFIG_PARROT_LINUXLOG),y)
libparrot-objs += parrot/ulog/src/ulog_ctrl.o

ccflags-y += -I$(parrottree)/packages/ulog/threadx/ulog_shd/include
ccflags-y += -I$(srctree)/vendors/ambarella/inc/ssp/threadx
endif

#obj-y += libulog.a
#install-files += libulog.a
