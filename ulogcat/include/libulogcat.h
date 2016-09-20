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

#ifndef _LIBULOGCAT_H
#define _LIBULOGCAT_H

#include <poll.h>

#define LIBULOGCAT_VERSION 2

enum ulogcat_format {
	ULOGCAT_FORMAT_SHORT,
	ULOGCAT_FORMAT_ALIGNED,
	ULOGCAT_FORMAT_PROCESS,
	ULOGCAT_FORMAT_LONG,
	ULOGCAT_FORMAT_CSV,
	ULOGCAT_FORMAT_BINARY,
	ULOGCAT_FORMAT_CKCM,
};

struct ulogcat_context;

/* v1 context creation interface (deprecated) */
struct ulogcat_opts {
	enum ulogcat_format opt_format;      /* output format */
	int                 opt_binary;      /* set to 1 for binary output */
	int                 opt_clear;       /* set to 1 to clear buffers */
	int                 opt_tail;        /* limit lines, 0 = no limit */
	int                 opt_getsize;     /* dump buffer stats on stdout */
	int                 opt_rotate_size; /* rotating file size in bytes */
	int                 opt_rotate_logs; /* number of rotating files */
	const char         *opt_rotate_filename; /* rotating base filename */
	int                 opt_dump;        /* dump buffers, do not block */
	int                 opt_color;       /* enable color output */
	int                 output_fd;       /* output file descriptor */
};
struct ulogcat_context *ulogcat_create(const struct ulogcat_opts *opts);


/* v2 context creation interface */
#define ULOGCAT_FLAG_CLEAR      (1 << 0)
#define ULOGCAT_FLAG_GET_SIZE   (1 << 1)
#define ULOGCAT_FLAG_DUMP       (1 << 2)
#define ULOGCAT_FLAG_COLOR      (1 << 3)
#define ULOGCAT_FLAG_SHOW_LABEL (1 << 4)
#define ULOGCAT_FLAG_ULOG       (1 << 5)
#define ULOGCAT_FLAG_ALOG       (1 << 6)
#define ULOGCAT_FLAG_KLOG       (1 << 7)

typedef void (*ulogcat_output_handler_t)(void *, unsigned char *, unsigned);

struct ulogcat_opts_v2 {
	enum ulogcat_format      opt_format;
	unsigned int             opt_flags;
	int                      opt_output_fd;
	ulogcat_output_handler_t opt_output_handler;
	void                    *opt_output_handler_data;
};
struct ulogcat_context *ulogcat_create2(const struct ulogcat_opts_v2 *opts);

int ulogcat_add_device(struct ulogcat_context *ctx, const char *name);
int ulogcat_process_logs(struct ulogcat_context *ctx);
void ulogcat_destroy(struct ulogcat_context *ctx);
const char *ulogcat_strerror(struct ulogcat_context *ctx);

/* v2 additional API */

/**
 * Add a specific Android buffer ('main', 'system', etc)
 */
int ulogcat_add_android_device(struct ulogcat_context *ctx, const char *name);

int ulogcat_init(struct ulogcat_context *ctx);
int ulogcat_get_descriptor_nb(struct ulogcat_context *ctx);
int ulogcat_setup_descriptors(struct ulogcat_context *ctx,
			      struct pollfd *fds, int nfds);
/* Returns:
 * 0 if data still to be processed
 * 1 if no more data available
 * -1 if an error occurred
 */
int ulogcat_process_descriptors(struct ulogcat_context *ctx,
				int poll_result, int *timeout_ms);

void ulogcat_flush_descriptors(struct ulogcat_context *ctx);

#endif /* _LIBULOGCAT_H */
