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

#define LIBULOGCAT_VERSION 3

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

/* v3 compat api */

/* opaque structure */
struct ulogcat3_context;

struct ulogcat_opts_v3 {
	enum ulogcat_format      opt_format;      /* output format */
	unsigned int             opt_flags;       /* see ULOGCAT_FLAG_* */
	unsigned int             opt_tail;        /* not working */
	FILE                    *opt_output_fp;   /* not supported */
	int                      opt_output_fd;   /* output descriptor (int) */
};

/**
 * Create a new libulogcat context.
 *
 * @param opts: log output options
 * Field @opt_format specifies how log messages should be rendered
 * Field @opt_flags is OR-mask specifying processing options. See ULOGCAT_FLAG_*
 * for details. If ULOGCAT_FLAG_DUMP is specified, only currently present logs
 * are processed, in a non-blocking way.
 * Field @opt_tail specifies the number of trailing lines that should be
 * displayed. A value of 0 means print all lines.
 * Field @opt_output_fp is the default output descriptor used for outputting
 * rendered log lines.
 * Field @opt_output_fd is the fallback output descriptor; it is used only if
 * opt_output_fp is NULL.
 * @param ulog_devices: array of ulog device names that should be processed; it
 * is only accessed during this call and does not need to be persistent. It
 * should contain names without the '/dev/ulog_' prefix, such as
 * 'main', 'pimp', etc. If no device name is specified (@param len = 0) and
 * flag ULOGCAT_FLAG_ULOG is specified in @param opts, then all ulog devices
 * are added.
 * @param len: number of elements in array ulog_devices.
 * @return: context structure or NULL upon error.
 */
struct ulogcat3_context *ulogcat3_open(const struct ulogcat_opts_v3 *opts,
				       const char **ulog_devices, int len);

/**
 * Destroy a context returned by ulogcat3_open().
 *
 * @param ctx: ulogcat context
 */
void ulogcat3_close(struct ulogcat3_context *ctx);

/**
 * Clear all log buffers specified in context.
 *
 * @param ctx: ulogcat context
 * @return: 0 if successful, a negative value in case of error
 */
int ulogcat3_clear(struct ulogcat3_context *ctx);

/**
 * Process log entries.
 *
 * Read, render and output entries from log devices.
 * If @max_entries is > 0, limit the number of processed entries to that number.
 * This function may block if flag ULOGCAT_FLAG_DUMP was not specified
 * in options.
 *
 * @param ctx: ulogcat context
 * @param max_entries: maximum number of processed lines, 0 means no limit
 *
 * @return: 0 if all entries have been processed
 *          1 if more entries need processing
 *          negative value if an error occured
 */
int ulogcat3_process_logs(struct ulogcat3_context *ctx, int max_entries);


#endif /* _LIBULOGCAT_H */
