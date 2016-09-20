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

/* DJB2 hash function */
static uint32_t hashbuf(uint8_t *buf, unsigned int size)
{
	unsigned int i;
	uint32_t hash = 5381;

	for (i = 0; i < size; i++)
		hash = 33*hash + *buf++;
	return hash;
}

int binary_frame_size(void)
{
	/* binary header with hash + full frame + terminating null byte */
	return 8+ULOGGER_ENTRY_MAX_LEN+1;
}

int render_binary_frame(struct ulogcat_context *ctx,
			const struct log_entry *_entry, struct frame *frame)
{
	int i, n = 0;
	uint32_t priority;
	struct ulogger_entry uentry;
	struct iovec vec[8];
	uint8_t *ptr;
	const struct ulog_entry *entry = &_entry->ulog;
	struct {
		uint8_t  magic[4];
		uint32_t hash;
	} hdr = {{'U', 'L', 'O', 'G'}, 0};

	priority = entry->priority & ULOG_PRIO_LEVEL_MASK;
	priority |= entry->is_binary ? (1 << ULOG_PRIO_BINARY_SHIFT) : 0;
	priority |= entry->color << ULOG_PRIO_COLOR_SHIFT;

	/* prefix header to ease resynchronization */
	vec[n].iov_base = (void *)&hdr;
	vec[n++].iov_len = sizeof(hdr);

	/* ulogger header */
	vec[n].iov_base = (void *)&uentry;
	vec[n++].iov_len = sizeof(uentry);

	/* process name */
	vec[n].iov_base = (void *)entry->pname;
	vec[n++].iov_len = strlen(entry->pname)+1;

	if (entry->pid != entry->tid) {
		/* optional thread name */
		vec[n].iov_base = (void *)entry->tname;
		vec[n++].iov_len = strlen(entry->tname)+1;
	}

	/* priority */
	vec[n].iov_base = (void *)&priority;
	vec[n++].iov_len = sizeof(priority);

	/* tag */
	vec[n].iov_base = (void *)entry->tag;
	vec[n++].iov_len = strlen(entry->tag)+1;

	/* message */
	vec[n].iov_base = (void *)entry->message;
	vec[n++].iov_len = entry->len;

	/* rebuild a proper ulogger header */
	uentry.len = 0;
	/* skip binary header and ulogger header */
	for (i = 2; i < n; i++)
		uentry.len += vec[i].iov_len;

	uentry.hdr_size = sizeof(uentry);
	uentry.pid = entry->pid;
	uentry.tid = entry->tid;
	uentry.sec = entry->tv_sec;
	uentry.nsec = entry->tv_nsec;
	/* use euid entry to encode frame origin */
	uentry.euid = _entry->label;

	hdr.hash = htole32(hashbuf((uint8_t *)&uentry, sizeof(uentry)));

	/* copy vector to frame buffer */
	ptr = frame->buf;
	frame->size = 0;
	for (i = 0; i < n; i++) {
		if ((int)(frame->size+vec[i].iov_len) > ctx->render_frame_size)
			return -1;
		memcpy(ptr, vec[i].iov_base, vec[i].iov_len);
		frame->size += vec[i].iov_len;
		ptr += vec[i].iov_len;
	}

	return 0;
}
