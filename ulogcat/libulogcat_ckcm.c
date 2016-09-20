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

/* We do not want to depend on CKCM headers */
#define UART_RT_STR             0x02
#define UART_RT_COLOR           0x24
#define UART_RT_FRAME_START1    0xd5
#define UART_RT_FRAME_END1      0xe5
#define UART_RT_FRAME_START2    0xe6
#define UART_RT_PLOG            0xe7
#define UART_RT_FRAME_END2      0xf6

enum CKCM_PLOG_OPT_FIELD {
	CKCM_PLOG_OPT_FIELD_COLOR = 0,
	CKCM_PLOG_OPT_FIELD_PC,
	CKCM_PLOG_OPT_FIELD_DATE,
	CKCM_PLOG_OPT_FIELD_TID,
	CKCM_PLOG_OPT_FIELD_TPRIO,
	CKCM_PLOG_OPT_FIELD_TNAME,
	CKCM_PLOG_OPT_FIELD_LAYERNAME,
};

int ckcm_frame_size(void)
{
	/* The estimated size of a full-fledged PLOG header+data CKCM frame */
	return 256+88;
}

/* Build a PLOG CKCM frame from a parsed ulog entry */
int render_ckcm_frame(struct ulogcat_context *ctx,
		      const struct log_entry *_entry, struct frame *frame)
{
	uint64_t usecs;
	int size, len, len2, opt, space;
	static const char cprio[8] = {
		[0]           = 'C',
		[1]           = 'C',
		[ULOG_CRIT]   = 'C',
		[ULOG_ERR]    = 'E',
		[ULOG_WARN]   = 'W',
		[ULOG_NOTICE] = 'I', /* FIXME: wxCKCM does not know NOTICE */
		[ULOG_INFO]   = 'I',
		[ULOG_DEBUG]  = 'D'
	};
	unsigned char *buf = frame->buf;
	const struct ulog_entry *entry = &_entry->ulog;

	/*
	 * available space in frame buffer: reserve bytes for bottom overhead:
	 * 2 (end) + 2 (start) + 4 (color) + 1 (RT_STR) + 1 (len) + 2 (end)
	 */
	space = ctx->render_frame_size - 12;

	size = 0;
	/* header part */
	buf[size++] = UART_RT_FRAME_START1;
	buf[size++] = UART_RT_FRAME_START2;
	buf[size++] = UART_RT_PLOG;

	/* reset plog optional field */
	opt = size;
	buf[size++] = ((1 << CKCM_PLOG_OPT_FIELD_DATE)|
		       (1 << CKCM_PLOG_OPT_FIELD_TID));

	/* log level */
	buf[size++] = cprio[entry->priority];

	/* optional color frame */
	if (entry->color) {
		buf[opt] |= (1 << CKCM_PLOG_OPT_FIELD_COLOR);
		buf[size++] = (entry->color >> 16) & 0xff; /* R */
		buf[size++] = (entry->color >>  8) & 0xff; /* G */
		buf[size++] = (entry->color >>  0) & 0xff; /* B */
	}

	/* date */
	usecs = frame->stamp;
	memcpy(&buf[size], &usecs, sizeof(usecs));
	size += sizeof(usecs);

	/* thread id */
	memcpy(&buf[size], &entry->tid, sizeof(entry->tid));
	size += sizeof(entry->tid);

	/* thread name */
	len = strlen(entry->tname);

	/* optionally prepend process name (if different from thread name) */
	len2 = (entry->tid != entry->pid) ? strlen(entry->pname)+1 : 0;
	if (size+len+len2 > space)
		/* drop frame */
		return -1;

	if (len+len2 > 0) {
		buf[opt] |= (1 << CKCM_PLOG_OPT_FIELD_TNAME);
		buf[size++] = (unsigned char)(len+len2);
		if (len2) {
			memcpy(&buf[size], entry->pname, len2-1);
			size += len2-1;
			buf[size++] = '/';
		}
		memcpy(&buf[size], entry->tname, len);
		size += len;
	}

	/* layer name */
	len = strlen(entry->tag);
	len += (ctx->flags & ULOGCAT_FLAG_SHOW_LABEL) ? 2 : 0;
	if (size+len > space)
		return -1;

	if (len > 0) {
		buf[opt] |= (1 << CKCM_PLOG_OPT_FIELD_LAYERNAME);
		buf[size++] = (unsigned char)len;
		/* optionally prepend desambiguation mark */
		if (ctx->flags & ULOGCAT_FLAG_SHOW_LABEL) {
			/* keep this in sync with len computation above */
			buf[size++] = _entry->label;
			buf[size++] = ':';
			len -= 2;
		}
		memcpy(&buf[size], entry->tag, len);
		size += len;
	}

	buf[size++] = UART_RT_FRAME_END1;
	buf[size++] = UART_RT_FRAME_END2;

	/* data part */
	buf[size++] = UART_RT_FRAME_START1;
	buf[size++] = UART_RT_FRAME_START2;

	space += 4;

	if (!entry->is_binary) {
		/* for CKCM old version compat, add again string color here */
		if (entry->color) {
			buf[size++] = UART_RT_COLOR;
			buf[size++] = ((entry->color >> 16) & 0xff) ? : 1;/*R*/
			buf[size++] = ((entry->color >>  8) & 0xff) ? : 1;/*G*/
			buf[size++] = ((entry->color >>  0) & 0xff) ? : 1;/*B*/
		}
		buf[size++] = UART_RT_STR;
		space += 5;

		/* truncate string if necessary */
		len = (entry->len-1 <= space) ? entry->len-1 : space;
		len = (len <= 255) ? len : 255;
		buf[size++] = len;
	} else {
		if (size+entry->len > space)
			/* do not bother truncating binary frames */
			return -1;
		len = entry->len;
	}

	/* frame actual payload */
	memcpy(&buf[size], entry->message, len);
	size += len;

	buf[size++] = UART_RT_FRAME_END1;
	buf[size++] = UART_RT_FRAME_END2;

	frame->size = size;
	return 0;
}
