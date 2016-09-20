/**
 * Copyright (C) 2014 Parrot S.A.
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
 *
 * libulogcat, a reader library for logger/ulogger/kernel log buffers
 *
 */

#include "libulogcat_private.h"

/* we do not want to depend on Android headers */
#ifndef __LOGGERIO
#define __LOGGERIO	0xAE
#define LOGGER_GET_LOG_BUF_SIZE		_IO(__LOGGERIO, 1) /* size of log */
#define LOGGER_GET_LOG_LEN		_IO(__LOGGERIO, 2) /* used log len */
#define LOGGER_GET_NEXT_ENTRY_LEN	_IO(__LOGGERIO, 3) /* next entry len */
#define LOGGER_FLUSH_LOG		_IO(__LOGGERIO, 4) /* flush log */
#define LOGGER_GET_VERSION		_IO(__LOGGERIO, 5) /* abi version */
#define LOGGER_SET_VERSION		_IO(__LOGGERIO, 6) /* abi version */
#define LOGGER_ENTRY_MAX_LEN		(5*1024)

struct logger_entry_v2 {
	uint16_t    len;       /* length of the payload */
	uint16_t    hdr_size;  /* sizeof(struct logger_entry_v2) */
	int32_t     pid;       /* generating process's pid */
	int32_t     tid;       /* generating process's tid */
	int32_t     sec;       /* seconds since Epoch */
	int32_t     nsec;      /* nanoseconds */
	uint32_t    euid;      /* effective UID of logger */
	char        msg[0];    /* the entry's payload */
};
#endif /* __LOGGERIO */

union logger_buf {
	unsigned char          buf[LOGGER_ENTRY_MAX_LEN+1];
	struct logger_entry_v2 entry;
};

/* adapted from Android 4.2 */
static int alog_parse_buf(struct logger_entry_v2 *buf, struct ulog_entry *entry)
{
	static const uint8_t alog2ulog[8] = {
		[0] = ULOG_DEBUG, /* ANDROID_LOG_UNKNOWN */
		[1] = ULOG_INFO,  /* ANDROID_LOG_DEFAULT */
		[2] = ULOG_DEBUG, /* ANDROID_LOG_VERBOSE */
		[3] = ULOG_DEBUG, /* ANDROID_LOG_DEBUG */
		[4] = ULOG_INFO,  /* ANDROID_LOG_INFO */
		[5] = ULOG_WARN,  /* ANDROID_LOG_WARN */
		[6] = ULOG_ERR,   /* ANDROID_LOG_ERROR */
		[7] = ULOG_CRIT,  /* ANDROID_LOG_FATAL */
	};

	entry->tv_sec  = buf->sec;
	entry->tv_nsec = buf->nsec;
	entry->pid = buf->pid;
	entry->tid = buf->tid;

	/*
	 * format: <priority:1><tag:N>\0<message:N>\0
	 *
	 * tag str
	 *   starts at buf->msg+1
	 * msg
	 *   starts at buf->msg+1+len(tag)+1
	 *
	 * The message may have been truncated by the kernel log driver.
	 * When that happens, we must null-terminate the message ourselves.
	 */
	if (buf->len < 3) {
		DEBUG("alog: message too short\n");
		/*
		 * A well-formed entry must consist of at least a priority
		 * and two null characters.
		 */
		return -1;
	}

	int msgStart = -1;
	int msgEnd = -1;

	int i;
	for (i = 1; i < buf->len; i++) {
		if (buf->msg[i] == '\0') {
			if (msgStart == -1) {
				msgStart = i + 1;
			} else {
				msgEnd = i;
				break;
			}
		}
	}

	if (msgStart == -1) {
		DEBUG("alog: malformed line\n");
		return -1;
	}
	if (msgEnd == -1) {
		/* incoming message not null-terminated; force it */
		msgEnd = buf->len - 1;
		buf->msg[msgEnd] = '\0';
	}

	entry->priority = alog2ulog[buf->msg[0] & 0x7];
	entry->tag = buf->msg + 1;
	entry->message = buf->msg + msgStart;
	entry->len = msgEnd - msgStart + 1;
	entry->is_binary = 0;
	entry->color = 0xffffff;
	entry->pname = "";
	entry->tname = "";

	return 0;
}

static int alog_receive_entry(struct log_device *dev, struct ulog_entry *entry)
{
	int ret;
	union logger_buf *raw = dev->priv;
	const int header_sz = (int)sizeof(struct logger_entry_v2);

	/* read exactly one logger entry */
	ret = read(dev->fd, raw->buf, LOGGER_ENTRY_MAX_LEN);
	if (ret < 0) {
		if ((errno == EINTR) || (errno == EAGAIN))
			return 0;
		set_error(dev->ctx, "read(%s): %s", dev->path,
			  strerror(errno));
		return -1;
	} else if (ret == 0) {
		set_error(dev->ctx, "read(%s): unexpected EOF", dev->path);
		return -1;
	} else if (raw->entry.len != ret-header_sz) {
		set_error(dev->ctx, "read(%s): unexpected length %d",
			  dev->path, ret-header_sz);
		return -1;
	}

	/* extract fields from raw entry */
	ret = alog_parse_buf(&raw->entry, entry);
	if (ret < 0)
		/* invalid entry? */
		return -1;

	return 1;
}

static int alog_clear_buffer(struct log_device *dev)
{
	int ret;

	ret = ioctl(dev->fd, LOGGER_FLUSH_LOG);
	if (ret < 0) {
		set_error(dev->ctx, "ioctl(%s): %s", dev->path,
			  strerror(errno));
	}
	return ret;
}

static int alog_get_size(struct log_device *dev, int *total, int *readable)
{
	*total = ioctl(dev->fd, LOGGER_GET_LOG_BUF_SIZE);
	if (*total < 0) {
		set_error(dev->ctx, "ioctl(%s): %s", dev->path,
			  strerror(errno));
		return -1;
	}

	*readable = ioctl(dev->fd, LOGGER_GET_LOG_LEN);
	if (*readable < 0) {
		set_error(dev->ctx, "ioctl(%s): %s", dev->path,
			  strerror(errno));
		return -1;
	}

	return 0;
}

int add_alog_device(struct ulogcat_context *ctx, const char *name)
{
	int mode, version;
	struct log_device *dev = NULL;

	dev = log_device_create(ctx);
	if (dev == NULL)
		goto fail;

	dev->priv = malloc(sizeof(union logger_buf));
	if (dev->priv == NULL)
		goto fail;

	mode = (ctx->flags & ULOGCAT_FLAG_CLEAR) ? O_RDWR : O_RDONLY;

	/* in Linux context, device inodes are /dev/log_xxx */
	snprintf(dev->path, sizeof(dev->path), "/dev/log_%s", name);
	dev->fd = open(dev->path, mode|O_NONBLOCK);
	if ((dev->fd < 0) && (errno != ENOENT)) {
		set_error(ctx, "cannot open Android device %s: %s", dev->path,
			  strerror(errno));
		goto fail;
	}

	if (dev->fd < 0) {
		/* in Android context, device inodes are /dev/log/xxx */
		snprintf(dev->path, sizeof(dev->path), "/dev/log/%s", name);
		dev->fd = open(dev->path, mode|O_NONBLOCK);
		if (dev->fd < 0) {
			set_error(ctx, "cannot open Android device %s: %s",
				  dev->path, strerror(errno));
			goto fail;
		}
	}

	version = 2;
	if (ioctl(dev->fd, LOGGER_SET_VERSION, &version)) {
		DEBUG("alog: cannot set Android log version: %s",
		      strerror(errno));
	}

	version = ioctl(dev->fd, LOGGER_GET_VERSION);
	if (version < 0)
		version = 1;

	/* we ignore euid/lid fields and do not distinguish v2 and v3*/
	if ((version != 2) && (version != 3)) {
		set_error(ctx, "unsupported Android log version: %d",
			  version);
		goto fail;
	}

	dev->receive_entry = alog_receive_entry;
	dev->clear_buffer = alog_clear_buffer;
	dev->get_size = alog_get_size;
	dev->label = 'A';
	ctx->alog_device_count++;
	return 0;

fail:
	log_device_destroy(dev);
	return -1;
}

int add_all_alog_devices(struct ulogcat_context *ctx)
{
	/* default to the following buffers (failure allowed) */
	(void)add_alog_device(ctx, "main");
	(void)add_alog_device(ctx, "system");
	return 0;
}
