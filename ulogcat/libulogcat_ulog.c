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
 *
 * libulogcat, a reader library for ulogger/kernel log buffers
 *
 */

#include "libulogcat_private.h"

/*
 * Read exactly one ulog entry.
 *
 * Returns -1 if an error occured
 *          0 if we received a signal and need to retry
 *          1 if we successfully read one entry
 */
static int ulog_receive_entry(struct log_device *dev, struct frame *frame)
{
	int ret;
	struct ulogger_entry *raw;
	const int header_sz = (int)sizeof(struct ulogger_entry);

	/* read exactly one ulogger entry */
	ret = read(dev->fd, frame->buf, frame->bufsize);
	if ((ret < 0) && (errno == EINVAL) && (frame->buf == frame->data)) {
		/* regular frame buffer is too small, allocate extra memory */
		frame->buf = malloc(ULOGGER_ENTRY_MAX_LEN);
		if (frame->buf == NULL) {
			INFO("malloc: %s\n", strerror(errno));
			return -1;
		}
		frame->bufsize = ULOGGER_ENTRY_MAX_LEN;
		/* retry with larger buffer */
		ret = read(dev->fd, frame->buf, frame->bufsize);
	}

	if (ret < 0) {
		if ((errno == EINTR) || (errno == EAGAIN))
			return 0;
		INFO("read(%s): %s\n", dev->path, strerror(errno));
		return -1;
	} else if (ret == 0) {
		INFO("read(%s): unexpected EOF\n", dev->path);
		return -1;
	}

	/* sanity check */
	raw = (struct ulogger_entry *)frame->buf;
	if (raw->len != ret-header_sz) {
		INFO("read(%s): unexpected length %d\n",
		     dev->path, ret-header_sz);
		return -1;
	}

	/*
	 * Extract fields from raw data: we would like to postpone this until
	 * rendering, but at the same time we need to filter out binary entries
	 * to correctly implement option -t (tail).
	 */
	ret = ulog_parse_buf(raw, &frame->entry);
	if (ret < 0) {
		DEBUG("ulog: dropping invalid message (error %d)\n", ret);
		return -1;
	}

	/* compute timestamp */
	frame->stamp = raw->sec*1000000ULL + raw->nsec/1000ULL;
	/* attach frame to device */
	frame->dev = dev;
	/* decrement read size except for special "dropped entries" messages */
	if ((raw->pid != -1) || (raw->tid != -1))
		dev->mark_readable -= ret;

	/* peek into data to drop non-displayable entries */
	if (frame->entry.is_binary &&
	    (dev->ctx->log_format != ULOGCAT_FORMAT_CSV))
		return 0;

	return 1;
}

static int ulog_parse_entry(struct frame *frame)
{
	/* no-op, parsing is now done earlier (upon entry read) */
	return 0;
}

static int ulog_parse_kmsgd_entry(struct frame *frame)
{
	kmsgd_fix_entry(&frame->entry);
	return 0;
}

static int ulog_clear_buffer(struct log_device *dev)
{
	int ret = -1, fd;

	fd = open(dev->path, O_WRONLY|O_NONBLOCK);
	if (fd < 0) {
		INFO("cannot open %s: %s\n", dev->path, strerror(errno));
		goto fail;
	}

	ret = ioctl(fd, ULOGGER_FLUSH_LOG);
	if (ret < 0) {
		INFO("ioctl(%s, ULOGGER_FLUSH_LOG): %s\n",
		     dev->path, strerror(errno));
	}
fail:
	if (fd >= 0)
		close(fd);
	return ret;
}

int add_ulog_device(struct ulogcat3_context *ctx, const char *name)
{
	struct log_device *dev = NULL;

	dev = log_device_create(ctx);
	if (dev == NULL)
		goto fail;

	snprintf(dev->path, sizeof(dev->path), "/dev/ulog_%s", name);

	dev->fd = open(dev->path, O_RDONLY|O_NONBLOCK);
	if (dev->fd < 0) {
		INFO("cannot open %s: %s\n", dev->path, strerror(errno));
		goto fail;
	}

	dev->receive_entry = ulog_receive_entry;
	dev->parse_entry = ulog_parse_entry;
	dev->clear_buffer = ulog_clear_buffer;
	dev->label = 'U';
	ctx->ulog_device_count++;

	/* kmsgd buffer wraps kernel messages and requires more processing */
	if (strcmp(name, KMSGD_ULOG_NAME) == 0) {
		dev->parse_entry = ulog_parse_kmsgd_entry;
		dev->label = 'K';
		snprintf(dev->path, sizeof(dev->path), "/proc/kmsg");
		ctx->ulog_device_count--;
	}

	/* get amount of data already present in buffer */
	dev->mark_readable = (ssize_t)ioctl(dev->fd, ULOGGER_GET_LOG_LEN);
	if (dev->mark_readable < 0) {
		INFO("ioctl(%s, ULOGGER_GET_LOG_LEN): %s\n",
		     dev->path, strerror(errno));
		goto fail;
	}

	return 0;

fail:
	log_device_destroy(dev);
	return -1;
}

int add_all_ulog_devices(struct ulogcat3_context *ctx)
{
	FILE *fp;
	char *p, buf[32];
	const char *name;
	int ret = 0;

	/* retrieve list of dynamically created log devices */
	fp = fopen("/sys/devices/virtual/misc/ulog_main/logs", "r");
	if (fp) {
		while (fgets(buf, sizeof(buf), fp)) {
			p = strchr(buf, ' ');

			if (p && (strlen(buf) > 5)) {
				*p = '\0';
				/* skip 'ulog_' prefix in log name */
				name = buf+5;
				/* skip special kmsgd buffer */
				if (strcmp(name, KMSGD_ULOG_NAME) != 0)
					ret = add_ulog_device(ctx, name);
				if (ret)
					break;
			}
		}
		fclose(fp);
	} else {
		/* backward compatibility if attribute file is not present */
		ret = add_ulog_device(ctx, &ULOGGER_LOG_MAIN[5]);
	}

	return ret;
}
