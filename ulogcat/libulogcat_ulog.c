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

union ulogger_buf {
	unsigned char        buf[ULOGGER_ENTRY_MAX_LEN+1];
	struct ulogger_entry entry;
};

static int ulog_receive_entry(struct log_device *dev, struct ulog_entry *entry)
{
	int ret;
	union ulogger_buf *raw = dev->priv;
	const int header_sz = (int)sizeof(struct ulogger_entry);

	/* read exactly one ulogger entry */
	ret = read(dev->fd, raw->buf, ULOGGER_ENTRY_MAX_LEN);
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
	ret = ulog_parse_buf(&raw->entry, entry);
	if (ret < 0) {
		DEBUG("ulog: dropping invalid message (error %d)\n", ret);
		return 0;
	}

	return 1;
}

static int ulog_receive_kmsgd_entry(struct log_device *dev,
				    struct ulog_entry *entry)
{
	int ret;

	ret = ulog_receive_entry(dev, entry);
	if (ret == 1)
		kmsgd_fix_entry(entry);

	return ret;
}

static int ulog_clear_buffer(struct log_device *dev)
{
	int ret;

	ret = ioctl(dev->fd, ULOGGER_FLUSH_LOG);
	if (ret < 0) {
		set_error(dev->ctx, "ioctl(%s): %s", dev->path,
			  strerror(errno));
	}
	return ret;
}

static int ulog_get_size(struct log_device *dev, int *total, int *readable)
{
	*total = ioctl(dev->fd, ULOGGER_GET_LOG_BUF_SIZE);
	if (*total < 0) {
		set_error(dev->ctx, "ioctl(%s): %s", dev->path,
			  strerror(errno));
		return -1;
	}

	*readable = ioctl(dev->fd, ULOGGER_GET_LOG_LEN);
	if (*readable < 0) {
		set_error(dev->ctx, "ioctl(%s): %s", dev->path,
			  strerror(errno));
		return -1;
	}

	return 0;
}

int add_ulog_device(struct ulogcat_context *ctx, const char *name)
{
	int mode;
	struct log_device *dev = NULL;

	dev = log_device_create(ctx);
	if (dev == NULL)
		goto fail;

	dev->priv = malloc(sizeof(union ulogger_buf));
	if (dev->priv == NULL)
		goto fail;

	snprintf(dev->path, sizeof(dev->path), "/dev/ulog_%s", name);

	mode = (ctx->flags & ULOGCAT_FLAG_CLEAR) ? O_WRONLY : O_RDONLY;

	dev->fd = open(dev->path, mode|O_NONBLOCK);
	if (dev->fd < 0) {
		set_error(ctx, "cannot open %s: %s", dev->path,
			  strerror(errno));
		goto fail;
	}

	dev->receive_entry = ulog_receive_entry;
	dev->clear_buffer = ulog_clear_buffer;
	dev->get_size = ulog_get_size;
	dev->label = 'U';
	ctx->ulog_device_count++;

	/* kmsgd buffer wraps kernel messages and requires more processing */
	if (strcmp(name, KMSGD_ULOG_NAME) == 0) {
		dev->receive_entry = ulog_receive_kmsgd_entry;
		dev->label = 'K';
		snprintf(dev->path, sizeof(dev->path), "/proc/kmsg");
		ctx->ulog_device_count--;
	}

	return 0;

fail:
	log_device_destroy(dev);
	return -1;
}

int add_all_ulog_devices(struct ulogcat_context *ctx)
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
