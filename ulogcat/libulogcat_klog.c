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
