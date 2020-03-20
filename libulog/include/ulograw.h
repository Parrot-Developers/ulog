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
 * libulog: a minimalistic logging library derived from Android logger
 *
 */

#ifndef _PARROT_ULOGRAW_H
#define _PARROT_ULOGRAW_H

#include <stdint.h>
#include <sys/uio.h>
#include <ulog.h>
#include "ulogger.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ulogger raw log entry.
 *
 * All const char * fields should be null-terminated.
 *
 * NOTE:
 * You do not need to fill the following fields, their values are ignored:
 *
 *  entry.len
 *  entry.hdr_len
 */
struct ulog_raw_entry {
	struct ulogger_entry entry;       /* ulogger kernel header (see NOTE) */
	uint32_t             prio;        /* logging priority and flags */
	const char          *pname;       /* process name */
	const char          *tname;       /* thread name, ignored if pid==tid */
	const char          *tag;         /* entry tag */
	const char          *message;     /* message */
	unsigned int         pname_len;   /* strlen(pname)+1 */
	unsigned int         tname_len;   /* strlen(tname)+1 ign. if pid==tid */
	unsigned int         tag_len;     /* strlen(tag)+1 */
	unsigned int         message_len; /* message length in bytes */
};

/**
 * Open a ulogger device for logging in raw mode.
 *
 * @param device A ulogger device (e.g. "/dev/ulog_main"); can be NULL, in which
 *               case libulog opens the default device, or the device specified
 *               by environment variable ULOG_DEVICE.
 * @return       A valid file descriptor, -errno upon failure.
 */
int ulog_raw_open(const char *device);

/**
 * Close a device opened with @ulog_raw_open().
 *
 * @param fd     A descriptor returned by @ulog_raw_open().
 */
void ulog_raw_close(int fd);

/**
 * Log a raw ulog entry.
 *
 * Logging a raw entry allows you to override entry attributes which are
 * normally defined by the kernel driver: pid, tid, process and thread names,
 * timestamp, etc. This low-level API should only be used for importing
 * log entries from other systems. Log entries are directly written to ulogger
 * kernel ring buffers, without any filtering of priorities and reformatting.
 * Use with caution.
 *
 * @param fd     A descriptor returned by @ulog_raw_open().
 * @param raw    A raw ulog entry, see @ulog_raw_entry for details.
 * @return       0 if successful, -errno upon failure.
 */
int ulog_raw_log(int fd, const struct ulog_raw_entry *raw);

/**
 * Same as ulog_raw_log but with the message specified as an array of iovec.
 *
 * @param fd     A descriptor returned by @ulog_raw_open().
 * @param raw    A raw ulog entry, see @ulog_raw_entry for details.
 * @return       0 if successful, -errno upon failure.
 */
int ulog_raw_logv(int fd, const struct ulog_raw_entry *raw,
		const struct iovec *iov,
		int iovcnt);

#ifdef __cplusplus
}
#endif

#endif /* _PARROT_ULOGRAW_H */
