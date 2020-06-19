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
#include "ulograw.h"
#include "ulogger.h"
#include "ulog_common.h"

ULOG_EXPORT int ulog_raw_open(const char *dev)
{
	const char *prop;
	char devbuf[32];
	struct stat st;
	int mode, ret, fd = -1;

	if (dev == NULL) {
		dev = "/dev/" ULOGGER_LOG_MAIN;
		prop = getenv("ULOG_DEVICE");
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

	/* switch to raw mode */
	mode = 1;
	ret = ioctl(fd, ULOGGER_SET_RAW_MODE, &mode);
	if (ret < 0) {
		/* assume feature is not present in driver */
		ret = -ENOSYS;
		goto fail;
	}

	return fd;
fail:
	if (fd >= 0)
		close(fd);
	return ret;
}

ULOG_EXPORT void ulog_raw_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

ULOG_EXPORT int ulog_raw_log(int fd, const struct ulog_raw_entry *raw)
{
	struct ulog_raw_entry tmp_raw;
	struct iovec vec[1];

	if (!raw)
		return -EINVAL;

	/* copy message in local iovec */
	vec[0].iov_base = (void *)raw->message;
	vec[0].iov_len = raw->message_len;

	/* copy raw structure and clear message */
	tmp_raw = *raw;
	tmp_raw.message = NULL;
	tmp_raw.message_len = 0;

	/* call version taking iovec array */
	return ulog_raw_logv(fd, &tmp_raw, vec, 1);
}

ULOG_EXPORT int ulog_raw_logv(int fd, const struct ulog_raw_entry *raw,
		const struct iovec *iov,
		int iovcnt)
{
	int i = 0, j;
	ssize_t ret;
	struct iovec vec[6 + iovcnt];
	const struct ulogger_entry *entry;
	const size_t prefix = sizeof((*entry).len) + sizeof((*entry).hdr_size);

	if ((fd < 0) || !raw || raw->message)
		return -EINVAL;

	entry = &raw->entry;

	/*
	 * Reject entries with both pid and tid equal to -1: these are used
	 * for pseudo-messages issued by the kernel driver to report dropped
	 * entries.
	 */
	if ((entry->pid == -1) && (entry->tid == -1))
		return -EINVAL;

	/*
	 * Computing entry->len and entry->hdr_len is not necessary, since the
	 * kernel will override those values anyway.
	 */

	/* header, skipping first 2 fields */
	vec[i].iov_base = (void *)((uint8_t *)entry + prefix);
	vec[i].iov_len = sizeof(*entry) - prefix;
	i++;

	/* process name, must be null-terminated */
	vec[i].iov_base = (void *)raw->pname;
	vec[i].iov_len = raw->pname_len;
	i++;
	if (entry->pid != entry->tid) {
		/* thread name, must be null-terminated */
		vec[i].iov_base = (void *)raw->tname;
		vec[i].iov_len = raw->tname_len;
		i++;
	}

	/* priority, color, binary flags, ... */
	vec[i].iov_base = (void *)&raw->prio;
	vec[i].iov_len = sizeof(raw->prio);
	i++;

	/* tag, must be null-terminated */
	vec[i].iov_base = (void *)raw->tag;
	vec[i].iov_len = raw->tag_len;
	i++;

	/* payload: null-terminated string or binary data */
	for (j = 0; j < iovcnt; j++) {
		vec[i].iov_base = iov[j].iov_base;
		vec[i].iov_len = iov[j].iov_len;
		i++;
	}

	/* send everything to kernel */
	do {
		ret = writev(fd, vec, i);
	} while ((ret < 0) && (errno == EINTR));

	return (ret < 0) ? -errno : 0;
}
