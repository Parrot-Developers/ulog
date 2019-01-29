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

#ifndef _LIBULOGCAT_H
#define _LIBULOGCAT_H

#define LIBULOGCAT_VERSION 3

/* text rendering available formats */
enum ulogcat_format {
	ULOGCAT_FORMAT_SHORT,
	ULOGCAT_FORMAT_ALIGNED,
	ULOGCAT_FORMAT_PROCESS,
	ULOGCAT_FORMAT_LONG,
	ULOGCAT_FORMAT_CSV,
};

/* opaque structure */
struct ulogcat3_context;

#define ULOGCAT_FLAG_DUMP       (1 << 2)  /* request non-blocking log dump */
#define ULOGCAT_FLAG_COLOR      (1 << 3)  /* request colored text output */
#define ULOGCAT_FLAG_SHOW_LABEL (1 << 4)  /* request label in text output */
#define ULOGCAT_FLAG_ULOG       (1 << 5)  /* request ulog devices */
#define ULOGCAT_FLAG_KLOG       (1 << 7)  /* request kernel messages */

struct ulogcat_opts_v3 {
	enum ulogcat_format      opt_format;      /* output format */
	unsigned int             opt_flags;       /* see ULOGCAT_FLAG_* */
	unsigned int             opt_tail;        /* show only tail lines */
	FILE                    *opt_output_fp;   /* output descriptor (FILE*)*/
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
 * This function may block if flag ULOGCAT_FLAG_DUMP was not specified in
 * options.
 *
 * @param ctx: ulogcat context
 * @param max_entries: maximum number of processed lines, 0 means no limit
 *
 * @return: 0 if all entries have been processed
 *          1 if more entries need processing
 *          negative value if an error occured
 */
int ulogcat3_process_logs(struct ulogcat3_context *ctx, int max_entries);

/* v1 API (deprecated) */
struct ulogcat_context;

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
int ulogcat_add_device(struct ulogcat_context *ctx, const char *name);
int ulogcat_process_logs(struct ulogcat_context *ctx);
void ulogcat_destroy(struct ulogcat_context *ctx);
const char *ulogcat_strerror(struct ulogcat_context *ctx);

/* v2 API has been removed (it was only used by ulogcat) */

#endif /* _LIBULOGCAT_H */
