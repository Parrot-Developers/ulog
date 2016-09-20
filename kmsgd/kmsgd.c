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
 * kmsgd, a daemon copying kernel messages to a ulog buffer
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/klog.h>

#define ULOG_TAG kmsgd
#include <ulog.h>

ULOG_DECLARE_TAG(kmsgd);

/*
 * This daemon logs raw kernel messages into a ulog buffer.
 * ulogcat later processes raw messages, including embedded kernel
 * timestamps.
 */
int main(void)
{
	char *p, *q;
	int offset = 0, len, count, size;
	static char buf[16384];

	while (1) {
		/* block until bytes are available */
		len = klogctl(2, &buf[offset], (int)sizeof(buf)-1-offset);
		if (len < 0) {
			fprintf(stderr, "klogctl(2): %s\n", strerror(errno));
			break;
		}

		size = offset+len;
		/* make sure string is nul-terminated */
		buf[size] = '\0';

		/* split buffer into lines */
		p = buf;
		count = 0;
		do {
			q = strchr(p, '\n');
			if (q) {
				*q++ = '\0';
				ULOG_STR(ULOG_INFO, p);
				count += q-p;
				p = q;
			}
		} while (q);

		if ((count > 0) && (count < size))
			/* move truncated line at buffer start */
			memmove(buf, &buf[count], size-count);
		offset = size-count;

		if (offset >= (int)sizeof(buf)-1)
			/* drop invalid (too long) line */
			offset = 0;
	}

	return 0;
}
