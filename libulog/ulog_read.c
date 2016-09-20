/**
 * Copyright (C) 2013 Parrot S.A.
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
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "ulog.h"
#include "ulogprint.h"
#include "ulogger.h"
#include "ulog_common.h"

static const char *get_token(const char **str, size_t *_size)
{
	size_t len, size = *_size;
	const char *p = *str;

	len = strnlen(p, size);
	if (len+1 > size)
		return NULL;

	*_size = size-len-1;
	*str = p+len+1;

	return p;
}

/* process an entry directly written to /dev/ulog_xxx */
static int ulog_parse_buf_unformatted(struct ulogger_entry *buf,
				      struct ulog_entry *entry,
				      char *p,
				      size_t size)
{
	int len;

	entry->priority = ULOG_INFO;
	entry->is_binary = 0;
	entry->color = 0;
	entry->tag = "";
	entry->message = p;

	len = strnlen(p, size);
	if (len == 0) {
		entry->message = "";
	} else {
		/* null-terminate string, buffer should be large enough */
		p[len] = '\0';
	}
	/* count trailing '\0' */
	entry->len = len+1;
	return 0;
}

ULOG_EXPORT int ulog_parse_buf(struct ulogger_entry *buf,
			       struct ulog_entry *entry)
{
	char *p;
	size_t size;

	entry->tv_sec  = buf->sec;
	entry->tv_nsec = buf->nsec;
	entry->pid     = buf->pid;
	entry->tid     = buf->tid;

	/*
	 * format: <pname:N>\0<tname:N>\0<priority:4><tag:N>\0<message:N>
	 *
	 * The message may have been truncated by the kernel log driver.
	 * When that happens, we must null-terminate the message ourselves.
	 */

	p = buf->msg;
	size = buf->len;

	/* process name */
	entry->pname = get_token((const char **)&p, &size);
	if (!entry->pname)
		return -1;

	/* thread name */
	if (entry->pid != entry->tid) {
		entry->tname = get_token((const char **)&p, &size);
		if (!entry->tname)
			return -1;
	} else {
		entry->tname = entry->pname;
	}

	/* priority, color, binary flag */
	if (size < 4)
		return ulog_parse_buf_unformatted(buf, entry, p, size);

	/* assume little-endian format */
	entry->priority  = ((uint32_t)p[0]) & ULOG_PRIO_LEVEL_MASK;
	entry->is_binary = !!(((uint32_t)p[0]) & (1 << ULOG_PRIO_BINARY_SHIFT));
	entry->color = (uint8_t)p[1]|((uint8_t)p[2] << 8)|((uint8_t)p[3] << 16);
	p += 4;
	size -= 4;

	/* tag */
	entry->tag = get_token((const char **)&p, &size);
	if (!entry->tag)
		return ulog_parse_buf_unformatted(buf, entry, p-4, size+4);

	/* message */
	entry->message = p;
	entry->len = size;

	if (!entry->is_binary) {
		/* message should be at least one byte */
		if (!size)
			return -1;

		entry->len = strnlen(p, size);
		if (entry->len == (int)size)
			/* null-terminate string */
			p[size-1] = '\0';
		else
			/* count trailing '\0' */
			entry->len++;
	}

	return 0;
}
