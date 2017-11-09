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

int text_render_size(void)
{
	return ULOGGER_ENTRY_MAX_LEN+128;
}

static const char *const ansinone = "\e[0m";
static const char priotab[8] = {' ', ' ', 'C', 'E', 'W', 'N', 'I', 'D'};

void setup_colors(struct ulogcat3_context *ctx)
{
	int i;
	char *p, *tmp;
	const char *colors, *seq;

	/* nice colors: export ULOGCAT_COLORS='||4;1;31|1;31|1;33|32||36' */

	/* use custom or default colors */
	colors = getenv("ULOGCAT_COLORS") ? : DEFAULT_COLORS;
	tmp = p = strdup(colors);

	for (i = 0; i < 8; i++) {
		seq = strsep(&tmp, "|");
		if (seq && seq[0]) {
			/* coverity[tainted_data] */
			snprintf(ctx->ansi_color[i], sizeof(ctx->ansi_color[i]),
				 "\e[%sm", seq);
		} else {
			ctx->ansi_color[i][0] = '\0';
		}
	}
	free(p);
}

static int print_log_line_csv(const struct frame *frame, char *buf,
			      size_t bufsize)
{
	char *p;
	int count, len, i;
	static const char hex[] = "0123456789abcdef";
	const struct ulog_entry *entry = &frame->entry;

	/* compute payload length */
	len = entry->is_binary ? 2*entry->len : (int)strlen(entry->message);

	/* prefix */
	count = snprintf(buf, bufsize,
			 "0x%08x,0x%08x,%d,0x%06x,%d,%s,%s,%d,%s,%d,%d,",
			 (unsigned int)entry->tv_sec,
			 (unsigned int)entry->tv_nsec,
			 entry->priority,
			 (unsigned int)entry->color,
			 entry->is_binary,
			 entry->tag,
			 entry->pname,
			 entry->pid,
			 entry->tname,
			 entry->tid,
			 len);

	if (count >= (int)bufsize)
		return -1;

	p = buf+count;

	/* we want to append len bytes + '\n' */
	count += len+1;

	if (count > (int)bufsize)
		return -1;

	if (entry->is_binary) {
		/* hex dump */
		for (i = 0; i < entry->len; i++) {
			unsigned char byte = (unsigned char)entry->message[i];
			/* coverity[overrun-local] */
			*p++ = hex[byte >> 4];
			*p++ = hex[byte & 0xf];
		}
	} else {
		/* copy string */
		memcpy(p, entry->message, len);
		p += len;
	}
	*p = '\n';

	return count;
}

static int print_ulog_line(const struct frame *frame, const char *message,
			   char *buf, size_t bufsize)
{
	int count;
	char cprio;
	struct tm tmBuf;
	struct tm *ptm;
	char tbuf[32], buf2[128];
	const char *cstart, *cend, *clabel;
	const struct ulog_entry *entry = &frame->entry;
	struct ulogcat3_context *ctx = frame->dev->ctx;

	cprio = priotab[entry->priority];
	cstart = (ctx->flags & ULOGCAT_FLAG_COLOR) ?
		ctx->ansi_color[entry->priority] : "";
	cend  = (ctx->flags & ULOGCAT_FLAG_COLOR) ? ansinone : "";
	clabel = (ctx->flags & ULOGCAT_FLAG_SHOW_LABEL) ? "U " : "";

	switch (ctx->log_format) {

	case ULOGCAT_FORMAT_SHORT:
		count = snprintf(buf, bufsize, "%s%s%c %-12s: %s%s\n", cstart,
				 clabel, cprio, entry->tag, message, cend);
		break;

	default:
	case ULOGCAT_FORMAT_ALIGNED:
		snprintf(buf2, sizeof(buf2), "%-12s(%s%s%s)",
			 entry->tag, entry->pname,
			 (entry->pid != entry->tid) ? "/" : "",
			 (entry->pid != entry->tid) ? entry->tname : "");
		count = snprintf(buf, bufsize, "%s%s%c %-45s: %s%s\n",
				 cstart, clabel, cprio, buf2, message, cend);
		break;

	case ULOGCAT_FORMAT_PROCESS:
		count = snprintf(buf, bufsize,
				 "%s%s%c %-12s(%s%s%s): %s%s\n", cstart,
				 clabel, cprio, entry->tag, entry->pname,
				 (entry->pid != entry->tid) ? "/" : "",
				 (entry->pid != entry->tid) ? entry->tname : "",
				 message, cend);
		break;

	case ULOGCAT_FORMAT_LONG:
		ptm = localtime_r(&(entry->tv_sec), &tmBuf);
		strftime(tbuf, sizeof(tbuf), "%m-%d %H:%M:%S", ptm);

		if (entry->pid != entry->tid) {
			snprintf(buf2, sizeof(buf2), "%-12s(%s-%d/%s-%d)",
				 entry->tag,
				 entry->pname, entry->pid,
				 entry->tname, entry->tid);
		} else {
			snprintf(buf2, sizeof(buf2), "%-12s(%s-%d)",
				 entry->tag, entry->pname, entry->pid);
		}

		count = snprintf(buf, bufsize,
				 "%s%s%s.%03ld %c %-45s: %s%s\n",
				 cstart, clabel, tbuf, entry->tv_nsec/1000000,
				 cprio, buf2, message, cend);
		break;
	}
	if (count >= (int)bufsize)
		/* message has been truncated */
		count = bufsize;

	return count;
}

static int print_klog_line(const struct frame *frame, const char *message,
			   char *buf, size_t bufsize)
{
	int count;
	char cprio;
	struct tm tmBuf;
	struct tm *ptm;
	char tbuf[32];
	const char *cstart, *cend, *clabel;
	const struct ulog_entry *entry = &frame->entry;
	struct ulogcat3_context *ctx = frame->dev->ctx;

	cprio = priotab[entry->priority];
	cstart = (ctx->flags & ULOGCAT_FLAG_COLOR) ?
		ctx->ansi_color[entry->priority] : "";
	cend  = (ctx->flags & ULOGCAT_FLAG_COLOR) ? ansinone : "";
	clabel = (ctx->flags & ULOGCAT_FLAG_SHOW_LABEL) ? "K " : "";

	switch (ctx->log_format) {

	case ULOGCAT_FORMAT_SHORT:
		count = snprintf(buf, bufsize, "%s%s%c %-12s: %s%s\n", cstart,
				 clabel, cprio, entry->tag, message, cend);
		break;

	default:
	case ULOGCAT_FORMAT_ALIGNED:
		count = snprintf(buf, bufsize, "%s%s%c %-45s: %s%s\n", cstart,
				 clabel, cprio, entry->tag, message, cend);
		break;

	case ULOGCAT_FORMAT_PROCESS:
		count = snprintf(buf, bufsize,
				 "%s%s%c %-12s: %s%s\n", cstart,
				 clabel, cprio, entry->tag, message, cend);
		break;

	case ULOGCAT_FORMAT_LONG:
		ptm = localtime_r(&(entry->tv_sec), &tmBuf);
		strftime(tbuf, sizeof(tbuf), "%m-%d %H:%M:%S", ptm);

		count = snprintf(buf, bufsize,
				 "%s%s%s.%03ld %c %-45s: %s%s\n",
				 cstart, clabel, tbuf, entry->tv_nsec/1000000,
				 cprio, entry->tag, message, cend);
		break;
	}
	if (count >= (int)bufsize)
		/* message has been truncated */
		count = bufsize;

	return count;
}

static int print_log_line(const struct frame *frame, const char *message,
			  char *buf, size_t bufsize)
{
	/* some buffers require specific processing */
	switch (frame->dev->label) {
	case 'K':
		return print_klog_line(frame, message, buf, bufsize);
	case 'U':
	default:
		return print_ulog_line(frame, message, buf, bufsize);
	}
	/* should not be reached */
	return -1;
}

int text_render_frame(struct ulogcat3_context *ctx, struct frame *frame,
		      int is_banner)
{
	int count, size;
	char *nl, *message, *p;

	size = ctx->render_size;

	if (is_banner) {
		/* crude banner rendering */
		count = snprintf((char *)ctx->render_buf, size,
				 "---------------------------------------%s\n",
				 frame->entry.message);
		ctx->render_len = count;
		return (count < 0) ? -1 : 0;
	}

	/* process CSV format separately */
	if (ctx->log_format == ULOGCAT_FORMAT_CSV) {
		count = print_log_line_csv(frame, (char *)ctx->render_buf,
					   size);
		ctx->render_len = count;
		return (count < 0) ? -1 : 0;
	}

	/* drop binary entries */
	if (frame->entry.is_binary)
		return -1;

	ctx->render_len = 0;
	p = (char *)ctx->render_buf;
	message = (char *)frame->entry.message;

	/* split lines in message */
	do {
		nl = strchr(message, '\n');
		if (nl)
			*nl = '\0';

		count = print_log_line(frame, message, p, size);
		if (count < 0)
			break;

		ctx->render_len += count;
		p += count;
		size -= count;

		if (nl)
			message = nl+1;

	} while (nl && message[0]);

	return ctx->render_len ? 0 : -1;
}
