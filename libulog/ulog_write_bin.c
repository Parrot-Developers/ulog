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

/* The ulog driver is only available on linux, but not on Android */
#if defined(__linux__) && !defined(__ANDROID__)
#	define FORCE_EXTERNAL_WRITE_FUNC 0
#else
#	define FORCE_EXTERNAL_WRITE_FUNC 1
#endif

static ulog_bin_write_func_t s_write_func;

ULOG_EXPORT int ulog_bin_open(const char *dev)
{
#if !FORCE_EXTERNAL_WRITE_FUNC
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
#else
	/* Since we cannot differenciate logs by device in this mode, we force
	 * callers to use the default device */
	if (dev != NULL)
		return -EINVAL;
	return 0;
#endif
}

ULOG_EXPORT void ulog_bin_close(int fd)
{
#if !FORCE_EXTERNAL_WRITE_FUNC
	if (fd >= 0)
		close(fd);
#else
	/* Nothing to do */
#endif
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
#if !FORCE_EXTERNAL_WRITE_FUNC
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
#else
	ulog_bin_write_func_t func;

	/* Handle custom write function if any, Assume this read is atomic */
	func = s_write_func;
	if (func != NULL) {
		(*func)(tag, tagsize, iov, iovcnt);
		return 0;
	}
	/* Otherwise, we can not log */
	return -ENOSYS;
#endif
}


/*
 * Each entry in binary ulog is limited to ULOGGER_ENTRY_MAX_PAYLOAD
 * (4076) bytes ie 4096 minus kernel fixed header
 * The kernel payload contains also:
 * - process/thread names: 2 * 16
 * - priority: 4
 * - tag (with null byte)
 * - header (optional)
 * - chunk idx: 1
 */
static size_t compute_max_chunk_len(size_t tagsize, size_t hdrlen)
{
	size_t extra =  2 * 16 + 4 + tagsize + hdrlen + 1;
	return extra < ULOGGER_ENTRY_MAX_PAYLOAD ?
		ULOGGER_ENTRY_MAX_PAYLOAD - extra : 0;
}

ULOG_EXPORT int ulog_bin_write_chunk(int fd,
	const char *tag,
	size_t tagsize,
	const void *hdr,
	size_t hdrlen,
	const void *buf,
	size_t count)
{
	struct iovec iov[1];
	iov[0].iov_base = (void *)buf;
	iov[0].iov_len = count;
	return ulog_bin_writev_chunk(fd, tag, tagsize, hdr, hdrlen, iov, 1);
}

ULOG_EXPORT int ulog_bin_writev_chunk(int fd,
	const char *tag,
	size_t tagsize,
	const void *hdr,
	size_t hdrlen,
	const struct iovec *iov,
	int iovcnt)
{
	int res = 0;
	size_t total = 0;
	size_t maxchunklen = compute_max_chunk_len(tagsize, hdrlen);
	uint8_t chunkidx = 0;
	size_t chunklen = 0;
	size_t chunkrem = 0;
	struct iovec iov2[iovcnt + 2];
	int i = 0;
	int iovcnt2 = 0;
	int iovidx = 0;
	size_t iovoff = 0;

	if (maxchunklen <= 0)
		return -EINVAL;

	/* Determine total size of the payload */
	for (i = 0; i < iovcnt; i++)
		total += iov[i].iov_len;


	while (total > 0) {
		chunklen = total < maxchunklen ? total : maxchunklen;

		iovcnt2 = 0;

		/* Optional header */
		if (hdrlen > 0) {
			iov2[iovcnt2].iov_base = (void *)hdr;
			iov2[iovcnt2].iov_len = hdrlen;
			iovcnt2++;
		}

		/* Chunk index */
		iov2[iovcnt2].iov_base = &chunkidx;
		iov2[iovcnt2].iov_len = sizeof(chunkidx);
		iovcnt2++;

		/* Chunk payload */
		chunkrem = chunklen;
		while (chunkrem > 0) {
			/* Setup iov */
			iov2[iovcnt2].iov_base = (uint8_t *)
				iov[iovidx].iov_base + iovoff;
			iov2[iovcnt2].iov_len = iov[iovidx].iov_len - iovoff <
				chunkrem ? iov[iovidx].iov_len - iovoff :
				chunkrem;

			/* Go to next source iov */
			iovoff += iov2[iovcnt2].iov_len;
			if (iovoff == iov[iovidx].iov_len) {
				iovidx++;
				iovoff = 0;
			}

			/* go to next dst iov */
			chunkrem -= iov2[iovcnt2].iov_len;
			iovcnt2++;
		}

		res = ulog_bin_writev(fd, tag, tagsize, iov2, iovcnt2);
		if (res < 0)
			return res;

		total -= chunklen;
		chunkidx++;
	}

	return 0;
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
