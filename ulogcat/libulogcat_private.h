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

#ifndef _LIBULOGCAT_PRIVATE_H
#define _LIBULOGCAT_PRIVATE_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>
#include <ctype.h>
#include <alloca.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/klog.h>

#include <ulog.h>
#include <ulogger.h>
#include <ulogprint.h>
#include <libulogcat.h>

#include "libulogcat_list.h"

/* Default colors used in text output mode */
#define DEFAULT_COLORS  "||4;1;31|1;31|1;33|35||1;30"

/*
 * This buffer contains wrapped kernel messages and should not be processed
 * as a regular ulog buffer
 */
#define KMSGD_ULOG_NAME "kmsgd"

#define INFO(...)        fprintf(stderr, "libulogcat: " __VA_ARGS__)

/*#define DEBUG(...)       fprintf(stderr, __VA_ARGS__)*/
#define DEBUG(...)       do {} while (0)

#define LIBULOGCAT_API  __attribute__((visibility("default")))

/*
 * Frame buffer size: this should be chosen large enough to contain most
 * messages. A typical ulogger kernel entry has the following size:
 *
 * size = sizeof(struct ulogger_entry) + strlen(pname)+1 + strlen(tname)+1 +
 *        sizeof(priority) + strlen(tag)+1 + strlen(message)
 *
 * The following assumptions should be true for most messages:
 *  * strlen(tag) <= 16
 *  * strlen(message) <= 120
 *
 * Hence: size <= 6*4 + 16 + 16 + 4 + 16 + 120 = 196
 *
 * If a message needs more than this size, an extra buffer will be allocated.
 */
#define ULOGCAT_FRAME_BUFSIZE   (200)

struct frame {
	struct log_device       *dev;         /* device that issued frame */
	struct listnode          flist;       /* queue to which frame belongs */
	struct ulog_entry        entry;       /* parsed ulog entry */
	uint8_t                 *buf;         /* pointer to raw data */
	size_t                   bufsize;     /* raw buffer size */
	uint64_t                 stamp;       /* message timestamp */
	uint8_t                  data[ULOGCAT_FRAME_BUFSIZE];
};

struct log_device;

typedef int (*ulogcat_recv_entry_t)(struct log_device *, struct frame *);
typedef int (*ulogcat_parse_entry_t)(struct frame *);
typedef int (*ulogcat_clear_buffer_t)(struct log_device *);

struct log_device {
	struct ulogcat3_context *ctx;
	char                     path[64];
	int                      fd;
	int                      idx;
	int                      printed;
	ssize_t                  mark_readable;
	struct listnode          queue;
	struct listnode          dlist;
	ulogcat_recv_entry_t     receive_entry;
	ulogcat_parse_entry_t    parse_entry;
	ulogcat_clear_buffer_t   clear_buffer;
	int                      pending;
	char                     label;
	void                    *priv;
};

struct ulogcat3_context {
	enum ulogcat_format      log_format;
	unsigned int             flags;
	int                      tail;
	char                     ansi_color[8][32];
	FILE                    *output_fp;
	int                      output_fd;
	int                      device_count;
	int                      pending;
	int                      render;
	struct pollfd           *fds;
	struct listnode          log_devices;
	struct listnode          free_queue;
	struct listnode          render_queue;
	struct listnode          pending_queue;
	struct frame            *frame_pool;
	uint8_t                 *render_buf;
	int                      render_size;
	int                      render_len;
	int                      ulog_device_count;
	int                      mark_reached;
	int                      output_error;
};

struct log_device *log_device_create(struct ulogcat3_context *ctx);
void log_device_destroy(struct log_device *dev);

int add_ulog_device(struct ulogcat3_context *ctx, const char *name);
void kmsgd_fix_entry(struct ulog_entry *entry);
int add_klog_device(struct ulogcat3_context *ctx);

int add_all_ulog_devices(struct ulogcat3_context *ctx);

int text_render_size(void);
int text_render_frame(struct ulogcat3_context *ctx, struct frame *frame,
		      int is_banner);

void setup_colors(struct ulogcat3_context *ctx);

#endif /* _LIBULOGCAT_PRIVATE_H */
