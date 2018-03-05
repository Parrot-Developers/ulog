/* include/linux/ulogger.h
 *
 * A fork of the Android Logger.
 *
 * Copyright (C) 2013 Parrot S.A.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _PARROT_ULOGGER_H
#define _PARROT_ULOGGER_H

#include <stdint.h>
#ifndef _WIN32
#  include <sys/ioctl.h>
#endif

/*
 * The userspace structure for version 1 of the ulogger_entry ABI.
 * This structure is returned to userspace unless the caller requests
 * an upgrade to a newer ABI version.
 */
struct user_ulogger_entry_compat {
	uint16_t	len;	/* length of the payload */
	uint16_t	__pad;	/* no matter what, we get 2 bytes of padding */
	int32_t		pid;	/* generating process's pid */
	int32_t		tid;	/* generating process's tid */
	int32_t		sec;	/* seconds since Epoch */
	int32_t		nsec;	/* nanoseconds */
	char		msg[0];	/* the entry's payload */
};

/*
 * The structure for version 2 of the ulogger_entry ABI.
 * This structure is returned to userspace if ioctl(ULOGGER_SET_VERSION)
 * is called with version >= 2
 */
struct ulogger_entry {
	uint16_t	len;		/* length of the payload */
	uint16_t	hdr_size;	/* sizeof(struct ulogger_entry_v2) */
	int32_t		pid;		/* generating process's pid */
	int32_t		tid;		/* generating process's tid */
	int32_t		sec;		/* seconds since Epoch */
	int32_t		nsec;		/* nanoseconds */
	int32_t		euid;		/* effective UID of ulogger */
	char		msg[0];		/* the entry's payload */
};

#define ULOGGER_LOG_MAIN	"ulog_main"	/* everything else */

/*
 * The maximum size of the log entry payload that can be
 * written to the kernel logger driver. An attempt to write
 * more than this amount to /dev/ulog_xxx will result in a
 * truncated log entry.
 */
#define ULOGGER_ENTRY_MAX_PAYLOAD	4076

/*
 * The maximum size of a log entry which can be read from the
 * kernel logger driver. An attempt to read less than this amount
 * may result in read() returning EINVAL.
 */
#define ULOGGER_ENTRY_MAX_LEN		(5*1024)

#define __ULOGGERIO	0xAE

#define ULOGGER_GET_LOG_BUF_SIZE	_IO(__ULOGGERIO, 21) /* size of log */
#define ULOGGER_GET_LOG_LEN		_IO(__ULOGGERIO, 22) /* used log len */
#define ULOGGER_GET_NEXT_ENTRY_LEN	_IO(__ULOGGERIO, 23) /* next entry len*/
#define ULOGGER_FLUSH_LOG		_IO(__ULOGGERIO, 24) /* flush log */
#define ULOGGER_GET_VERSION		_IO(__ULOGGERIO, 25) /* abi version */
#define ULOGGER_SET_VERSION		_IO(__ULOGGERIO, 26) /* abi version */
#define ULOGGER_SET_RAW_MODE		_IO(__ULOGGERIO, 27) /* write raw logs*/

#endif /* _PARROT_ULOGGER_H */
