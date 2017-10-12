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

enum ulogcat_format {
	ULOGCAT_FORMAT_SHORT,
	ULOGCAT_FORMAT_ALIGNED,
	ULOGCAT_FORMAT_PROCESS,
	ULOGCAT_FORMAT_LONG,
	ULOGCAT_FORMAT_CSV,
};

struct ulogcat3_context;

#define ULOGCAT_FLAG_DUMP       (1 << 2)
#define ULOGCAT_FLAG_COLOR      (1 << 3)
#define ULOGCAT_FLAG_SHOW_LABEL (1 << 4)
#define ULOGCAT_FLAG_ULOG       (1 << 5)
#define ULOGCAT_FLAG_KLOG       (1 << 7)

struct ulogcat_opts_v3 {
	enum ulogcat_format      opt_format;
	unsigned int             opt_flags;
	unsigned int             opt_tail;
	FILE                    *opt_output_fp;
	int                      opt_output_fd;
};

struct ulogcat3_context *ulogcat3_open(const struct ulogcat_opts_v3 *opts,
				       const char **ulog_devices, int len);
void ulogcat3_close(struct ulogcat3_context *ctx);
int ulogcat3_clear(struct ulogcat3_context *ctx);
int ulogcat3_process_logs(struct ulogcat3_context *ctx, int max_entries);
const char *ulogcat3_strerror(struct ulogcat3_context *ctx);


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
