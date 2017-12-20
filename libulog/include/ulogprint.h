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
 *
 * libulog: a minimalistic logging library derived from Android logger
 *
 */

#ifndef _PARROT_ULOGPRINT_H
#define _PARROT_ULOGPRINT_H

#include <stdint.h>
#include <time.h>

#include "ulogger.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ulog_entry {
	time_t      tv_sec;    /* seconds since Epoch */
	long        tv_nsec;   /* nanoseconds */
	int         priority;  /* logging priority: ULOG_CRIT, ..., ULOG_DEBUG*/
	int32_t     pid;       /* process (thread group leader) ID */
	const char *pname;     /* null-terminated process name */
	int32_t     tid;       /* thread ID */
	const char *tname;     /* null-terminated thread name */
	const char *tag;       /* null-terminated tag */
	const char *message;   /* message (null-terminated if is_binary == 0) */
	int         len;       /* message length, including null character */
	int         is_binary; /* set to 1 if msg is binary data, 0 otherwise */
	uint32_t    color;     /* 24-bit unsigned integer */
};

/**
 * Splits a wire-format buffer into a 'struct ulog_entry'
 * entry allocated by caller. Pointers will point directly into log buffer.
 * @buf should be at least of size ULOGGER_ENTRY_MAX_LEN+1.
 *
 * Returns 0 on success and -1 on invalid wire format (entry will be
 * in unspecified state)
 */
int ulog_parse_buf(struct ulogger_entry *buf, struct ulog_entry *entry);

/**
 * Parse and split a raw buffer into a 'struct ulog_entry' structure
 * allocated by caller. Pointers in the structure will point directly to
 * strings in the raw buffer.
 *
 * @param buf:   pointer to raw buffer
 * @param len:   raw buffer length in bytes
 * @param entry: output structure
 *
 * @return: 0 on success, -1 on invalid buffer (entry will be left in an
 * unspecified state)
 */
int ulog_parse_raw(void *buf, size_t len, struct ulog_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* _PARROT_ULOGPRINT_H */
