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
 * libulogcat, a reader library for ulogger/kernel log buffers
 *
 */

#include "libulogcat_private.h"

static void parse_prefix(struct ulog_entry *entry)
{
	int len;
	char *endp;
	const char *p;

	p = entry->message;

	if ((p[0] != '<') || (entry->len < 4))
		/* invalid line start */
		return;

	if (p[2] == '>') {
		entry->priority = isdigit(p[1]) ? (p[1]-'0') & 0x7 : ULOG_INFO;
		p += 3;
	} else {
		entry->priority = strtoul(p+1, &endp, 10) & 0x7;
		if ((endp == NULL) || (*endp != '>'))
			/* malformed line ? */
			return;
		p = endp+1;
	}

	len = p-entry->message;

	entry->message += len;
	entry->len -= len;
}

static void parse_timestamp(struct ulog_entry *entry)
{
	int len;
	char *endp;
	const char *p;

	p = entry->message;

	if (p[0] != '[')
		goto notimestamp;

	entry->tv_sec = strtoul(p+1, &endp, 10);
	if ((endp == NULL) || (*endp != '.'))
		/* malformed line ? */
		goto notimestamp;

	p = endp+1;

	entry->tv_nsec = strtoul(p, &endp, 10)*1000;
	if ((endp == NULL) || (endp[0] != ']') || (endp[1] != ' '))
		/* malformed line ? */
		goto notimestamp;

	len = endp+2 - entry->message;

	entry->message += len;
	entry->len -= len;
	return;

notimestamp:
	entry->tv_sec = 0;
	entry->tv_nsec = 0;
}

/* This function is used when reading kernel messages from a ulog device */
void kmsgd_fix_entry(struct ulog_entry *entry)
{
	parse_prefix(entry);
	parse_timestamp(entry);

	entry->pid = 0;
	entry->tid = 0;
	entry->pname = "";
	entry->tname = "";
	entry->tag = "KERNEL";
	entry->is_binary = 0;
	entry->color = 0;
}

static int klog_read_entry(struct log_device *dev, struct frame *frame)
{
	int ret;

	/* read exactly one kmsg entry */
	ret = read(dev->fd, frame->buf, frame->bufsize-1);
	if ((ret < 0) && (errno == EINVAL) && (frame->buf == frame->data)) {
		/* regular frame buffer is too small, allocate extra memory */
		frame->buf = malloc(BUFSIZ);
		if (frame->buf == NULL) {
			INFO("malloc: %s\n", strerror(errno));
			return -1;
		}
		frame->bufsize = BUFSIZ;
		/* retry with larger buffer */
		ret = read(dev->fd, frame->buf, frame->bufsize-1);
	}

	if (ret < 0) {
		/* EPIPE can be returned when message has been overwritten */
		if ((errno == EINTR) || (errno == EAGAIN) || (errno == EPIPE))
			return 0;
		INFO("read(%s): %s\n", dev->path, strerror(errno));
		return -1;
	}

	/* make sure buffer is null-terminated */
	frame->buf[ret] = '\0';
	return ret;
}

/*
 * Read exactly one kernel entry.
 *
 * Returns -1 if an error occured
 *          0 if we received a signal and need to retry
 *          1 if we successfully read one entry
 */
static int klog_receive_entry(struct log_device *dev, struct frame *frame)
{
	int ret;
	long long int usec;
	char *saveptr = NULL, *prio, *timestamp;
	struct ulog_entry *entry = &frame->entry;

	ret = klog_read_entry(dev, frame);
	if (ret <= 0)
		return ret;
	/*
	 * We need a timestamp now: partially parse entry (see
	 * klog_parse_entry() for details.
	 */
	prio = strtok_r((char *)frame->buf, ",", &saveptr);
	(void)strtok_r(NULL, ",", &saveptr);  /* skip seqnum */
	timestamp = strtok_r(NULL, ",", &saveptr);

	if (!prio || !timestamp)
		return -1;

	usec = strtoll(timestamp, NULL, 10);
	entry->priority = (int)(strtol(prio, NULL, 10) & 0x7L);
	entry->tv_sec = usec/1000000ULL;
	entry->tv_nsec = (usec - entry->tv_sec*1000000ULL)*1000;
	timestamp += strlen(timestamp) + 1;
	entry->message = strchr(timestamp, ';');
	if (!entry->message)
		return -1;

	frame->stamp = usec;
	/* attach frame to device */
	frame->dev = dev;
	dev->mark_readable -= ret;

	return 1;
}

static int hex2dec(char x)
{
	return (isdigit(x) ? x - '0' : tolower(x) - 'a' + 10) & 0xf;
}

/*
 * Parse a kmsg entry.
 *
 * Fields are separated by ',', and a ';' separator marks the beginning of the
 * message. Message non-printable bytes are escaped C-style (\xNN).
 *
 * Example:
 *  7,160,424069,-;pci_root PNP0A03:00: host bridge window [io  0x0000-0x0cf7]
 *  SUBSYSTEM=acpi
 *  DEVICE=+acpi:PNP0A03:00
 *  6,339,5140900,-;NET: Registered protocol family 10
 *  30,340,5690716,-;udevd[80]: starting version 181
 */
static int klog_parse_entry(struct frame *frame)
{
	char *p, *q;
	struct ulog_entry *entry = &frame->entry;

	/* we already partially parsed the record, continue... */
	entry->message++;
	p = strchr(entry->message, '\n');
	if (p == NULL)
		return -1;

	*p = '\0';
	entry->len = p - entry->message + 1;
	entry->pid = 0;
	entry->tid = 0;
	entry->pname = "";
	entry->tname = "";
	entry->tag = "KERNEL";
	entry->is_binary = 0;
	entry->color = 0;

	/* unescape message in place */
	p = strchr(entry->message, '\\');
	if (p) {
		q = p;
		while (*p) {
			/* unescape \xNN sequence */
			if ((p[0] == '\\') &&
			    (p[1] == 'x')  &&
			    isxdigit(p[2]) &&
			    isxdigit(p[3])) {
				*q++ = (hex2dec(p[2]) << 4)|hex2dec(p[3]);
				p += 4;
			} else {
				*q++ = *p++;
			}
		}
		*q = '\0';
		entry->len = q - entry->message + 1;
	}

	return 0;
}

static int klog_clear_buffer(struct log_device *dev)
{
	int ret;

	ret = klogctl(5/* SYSLOG_ACTION_CLEAR */, NULL, 0);
	if (ret < 0)
		INFO("klogctl(SYSLOG_ACTION_CLEAR): %s\n", strerror(errno));

	return ret;
}

int add_klog_device(struct ulogcat3_context *ctx)
{
	int ret;
	uint8_t *buf;
	struct log_device *dev;

	dev = log_device_create(ctx);
	if (dev == NULL)
		goto fail;

	snprintf(dev->path, sizeof(dev->path), "/dev/kmsg");

	/*
	 * Even if we successfully open the device, kernel may still not
	 * support reads from /dev/kmsg (typically returning -EINVAL on read());
	 * the only way to know for sure is to read at least once...
	 */
	dev->fd = open(dev->path, O_RDONLY|O_NONBLOCK);
	if (dev->fd < 0) {
		INFO("open %s: %s\n", dev->path, strerror(errno));
		goto fail;
	}

	/* skip stale entries */
	(void)lseek(dev->fd, 0, SEEK_DATA);

	buf = alloca(BUFSIZ);
	ret = read(dev->fd, buf, BUFSIZ);
	/* broken pipe error can be ignored since it just means
	 * that some messages have been overwritten. */
	if ((ret < 0) && (errno != EAGAIN) && (errno != EPIPE))
		/* OK, assume this kernel is too old */
		goto fail;

	/* rewind our descriptor */
	(void)lseek(dev->fd, 0, SEEK_DATA);

	dev->receive_entry = klog_receive_entry;
	dev->parse_entry = klog_parse_entry;
	dev->clear_buffer = klog_clear_buffer;
	dev->label = 'K';

	/*
	 * We cannot get a reliable readable size (because /dev/kmsg does some
	 * formatting on messages). Instead we use the total buffer size and
	 * multiply it by a factor to get something larger than the readable
	 * size.
	 */
	dev->mark_readable = 2*(ssize_t)klogctl(10/*SYSLOG_ACTION_SIZE_BUFFER*/,
						NULL, 0);
	if (dev->mark_readable < 0) {
		INFO("klogctl(SYSLOG_ACTION_SIZE_BUFFER): %s\n",
		     strerror(errno));
		goto fail;
	}

	return 0;
fail:
	log_device_destroy(dev);
	return -1;
}
