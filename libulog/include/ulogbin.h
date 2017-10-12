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

#ifndef _PARROT_ULOGBIN_H
#define _PARROT_ULOGBIN_H

#include <stdint.h>
#include <sys/uio.h>
#include <ulog.h>
#include "ulogger.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ULOG_BIN_DEFAULT	"ulog_mainbin"

/**
 * Open a ulogger device for logging in bin mode.
 *
 * @param device A ulogger device (e.g. "/dev/ulog_main"); can be NULL, in which
 *               case libulog opens the default device, or the device specified
 *               by environment variable ULOG_DEVICE_BIN.
 * @return       A valid file descriptor, -errno upon failure.
 */
int ulog_bin_open(const char *device);

/**
 * Close a device opened with @ulog_bin_open().
 *
 * @param fd     A descriptor returned by @ulog_bin_open().
 */
void ulog_bin_close(int fd);

/**
 * Log a binary ulog entry. The priority will be set to INFO and always logged
 *
 * @param fd      A descriptor returned by @ulog_bin_open().
 * @param tag     tag associated with the log.
 * @param tagsize tag length + 1 (so including nul byte).
 * @param buf     buffer to write.
 * @param count   size of buffer to write.
 * @return        0 if successful, -errno upon failure.
 */
int ulog_bin_write(int fd,
	const char *tag,
	size_t tagsize,
	const void *buf,
	size_t count);

/**
 * Log a binary ulog entry. The priority will be set to INFO and always logged
 *
 * @param fd      A descriptor returned by @ulog_bin_open().
 * @param tag     tag associated with the log.
 * @param tagsize tag length + 1 (so including nul byte).
 * @param iov     iovec array to write.
 * @param iovcnt  number of elements in iovec array.
 * @return        0 if successful, -errno upon failure.
 */
int ulog_bin_writev(int fd,
	const char *tag,
	size_t tagsize,
	const struct iovec *iov,
	int iovcnt);

#ifdef __cplusplus
}
#endif

#endif /* _PARROT_ULOGBIN_H */
