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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/eventfd.h>

#include <ulog.h>
#include <ulogger.h>
#include <ulogprint.h>
#include <libulogcat.h>

#include "libulogcat_list.h"

/* The maximum number of frames to buffer for a device */
#define MAX_PENDING_FRAMES    (64)

/* The maximum number of frames after which we stop flushing */
#define MAX_FLUSHED_FRAMES    (1000000)

/* The amount of milliseconds of idleness after which we flush all frames */
#define FRAME_IDLE_TIMEOUT_MS (50)

/* Default colors used in text output mode */
#define DEFAULT_COLORS  "||4;1;31|1;31|1;33|35||1;30"

/*
 * This buffer contains wrapped kernel messages and should not be processed
 * as a regular ulog buffer
 */
#define KMSGD_ULOG_NAME "kmsgd"

/*#define DEBUG(...)       fprintf(stderr, __VA_ARGS__)*/
#define DEBUG(...)       do {} while (0)

#define LIBULOGCAT_API  __attribute__((visibility("default")))

struct frame {
	uint64_t                 stamp;
	struct listnode          flist;
	int                      is_banner;
	unsigned int             size;
	unsigned char            buf[0];
};

struct log_entry {
	struct ulog_entry        ulog;
	char                     label;
};

struct log_device;

typedef int (*ulogcat_recv_entry_t)(struct log_device *, struct ulog_entry *);
typedef int (*ulogcat_render_frame_t)(struct ulogcat_context *,
				      const struct log_entry *,
				      struct frame *);
typedef int (*ulogcat_clear_buffer_t)(struct log_device *);
typedef int (*ulogcat_get_size_t)(struct log_device *, int *, int *);

enum log_device_state {
	ACTIVE,
	PAUSED,
	IDLE,
};

struct log_device {
	struct ulogcat_context  *ctx;
	enum log_device_state    state;
	char                     path[64];
	int                      fd;
	int                      idx;
	int                      printed;
	struct listnode          queue;
	struct listnode          dlist;
	ulogcat_recv_entry_t     receive_entry;
	ulogcat_clear_buffer_t   clear_buffer;
	ulogcat_get_size_t       get_size;
	int                      pending_frames;
	char                     label;
	void                    *priv;
};

struct ulogcat_context {
	enum ulogcat_format      log_format;
	unsigned int             flags;
	char                     ansi_color[8][32];
	int                      output_fd;
	ulogcat_output_handler_t output_handler;
	void                    *output_handler_data;
	off_t                    output_count;
	int                      device_count;
	struct pollfd           *fds;
	struct listnode          log_devices;
	struct listnode          free_frames;
	int                      queued_frames;
	ulogcat_render_frame_t   render_frame;
	int                      render_frame_size;
	int                      ulog_device_count;
	int                      alog_device_count;
	int                      control_fd[2];
	char                     last_error[128];
};

struct log_device *log_device_create(struct ulogcat_context *ctx);
void log_device_destroy(struct log_device *dev);
void set_error(struct ulogcat_context *ctx, const char *fmt, ...);

int add_ulog_device(struct ulogcat_context *ctx, const char *name);
int add_alog_device(struct ulogcat_context *ctx, const char *name);
void kmsgd_fix_entry(struct ulog_entry *entry);

int add_all_ulog_devices(struct ulogcat_context *ctx);
int add_all_alog_devices(struct ulogcat_context *ctx);

int ckcm_frame_size(void);
int render_ckcm_frame(struct ulogcat_context *ctx,
		      const struct log_entry *_entry, struct frame *frame);

int binary_frame_size(void);
int binary_full_frame_size(void);
int render_binary_frame(struct ulogcat_context *ctx,
			const struct log_entry *_entry, struct frame *frame);

int text_frame_size(void);
int render_text_frame(struct ulogcat_context *ctx,
		      const struct log_entry *_entry, struct frame *frame);

int output_rotate(struct ulogcat_context *ctx, struct frame *frame);
void flush_rotate(struct ulogcat_context *ctx);

void setup_colors(struct ulogcat_context *ctx);

#endif /* _LIBULOGCAT_PRIVATE_H */
