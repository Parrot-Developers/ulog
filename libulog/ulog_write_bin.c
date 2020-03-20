/**
 * Copyright (C) 2017 Parrot S.A.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * libulog: a minimalistic logging library derived from Android logger
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <fcntl.h>

#include "ulog.h"
#include "ulogbin.h"
#include "ulogger.h"
#include "ulog_common.h"

static ulog_bin_write_func_t s_write_func;

ULOG_EXPORT int ulog_bin_open(const char *dev)
{
	const char *prop;
	char devbuf[32];
	struct stat st;
	int ret, fd = -1;

	if (dev == NULL) {
		dev = "/dev/" ULOG_BIN_DEFAULT;
		prop = getenv("ULOG_DEVICE_BIN");
		if (prop) {
			snprintf(devbuf, sizeof(devbuf), "/dev/ulog_%s", prop);
			dev = devbuf;
		}
	}

	fd = open(dev, O_WRONLY|O_CLOEXEC);
	if (fd < 0) {
		ret = -errno;
		goto fail;
	}

	/* sanity check: /dev/ulog_* must be device files */
	if ((fstat(fd, &st) < 0) || !S_ISCHR(st.st_mode)) {
		ret = -EINVAL;
		goto fail;
	}

	return fd;
fail:
	if (fd >= 0)
		close(fd);
	return ret;
}

ULOG_EXPORT void ulog_bin_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

ULOG_EXPORT int ulog_bin_write(int fd,
	const char *tag,
	size_t tagsize,
	const void *buf,
	size_t count)
{
	struct iovec iov[1];
	iov[0].iov_base = (void *)buf;
	iov[0].iov_len = count;
	return ulog_bin_writev(fd, tag, tagsize, iov, 1);
}

ULOG_EXPORT int ulog_bin_writev(int fd,
	const char *tag,
	size_t tagsize,
	const struct iovec *iov,
	int iovcnt)
{
	ssize_t ret;
	uint32_t prio = ULOG_INFO | (1U << ULOG_PRIO_BINARY_SHIFT);
	struct iovec vec[2 + iovcnt];
	int i;
	ulog_bin_write_func_t func;

	/* Handle custom write function if any, Assume this read is atomic */
	func = s_write_func;
	if (func != NULL) {
		(*func)(tag, tagsize, iov, iovcnt);
		return 0;
	}

	/* priority, binary flags, ... */
	vec[0].iov_base = (void *)&prio;
	vec[0].iov_len = sizeof(prio);

	/* tag, must be null-terminated */
	vec[1].iov_base = (void *)tag;
	vec[1].iov_len = tagsize;

	/* payload: null-terminated string or binary data */
	for (i = 0; i < iovcnt; i++) {
		vec[2 + i].iov_base = iov[i].iov_base;
		vec[2 + i].iov_len = iov[i].iov_len;
	}

	/* send everything to kernel */
	do {
		ret = writev(fd, vec, 2 + iovcnt);
	} while ((ret < 0) && (errno == EINTR));

	return ret > 0 ? 0 : -errno;
}

ULOG_EXPORT int ulog_bin_set_write_func(ulog_bin_write_func_t func)
{
	/* Assume this write is atomic */
	s_write_func = func;
	return 0;
}

ulog_bin_write_func_t ulog_bin_get_write_func(void)
{
	/* Assume this read is atomic */
	return s_write_func;
}
