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

#include "libulogcat_private.h"

static void output_rendered(struct ulogcat3_context *ctx)
{
	ssize_t ret;

	if (ctx->output_error)
		return;

	if (ctx->output_fp) {
		ret = fwrite(ctx->render_buf, ctx->render_len, 1,
			     ctx->output_fp);
		if (ret != 1) {
			INFO("cannot output frame: %s\n", strerror(errno));
			ctx->output_error = 1;
		}
	} else if (ctx->output_fd >= 0) {
		do {
			ret = write(ctx->output_fd,
				    ctx->render_buf, ctx->render_len);
		} while ((ret < 0) && (errno == EINTR));

		if (ret < 0)
			ctx->output_error = 1;
	}
}

static struct frame *alloc_frame(struct ulogcat3_context *ctx)
{
	struct frame *frame;

	if (list_empty(&ctx->free_queue))
		/* out of frames, this should theoretically not happen */
		return NULL;

	/* reuse frame */
	frame = node_to_item(list_head(&ctx->free_queue), struct frame, flist);
	list_remove(&frame->flist);

	return frame;
}

static void free_frame(struct ulogcat3_context *ctx, struct frame *frame)
{
	if (frame) {
		if (frame->buf != frame->data) {
			/* free extra allocated memory */
			free(frame->buf);
			frame->buf = frame->data;
			frame->bufsize = sizeof(frame->data);
		}
		list_add_tail(&ctx->free_queue, &frame->flist);
	}
}

static struct frame *find_oldest_pending_frame(struct ulogcat3_context *ctx)
{
	struct listnode *node;
	struct frame *frame, *res = NULL;
	uint64_t stamp = 0ULL;

	list_for_each(node, &ctx->pending_queue) {
		frame = node_to_item(node, struct frame, flist);
		if ((res == NULL) || (frame->stamp < stamp)) {
			res = frame;
			stamp = frame->stamp;
		}
	}

	return res;
}

static int render_frame(struct ulogcat3_context *ctx, struct frame *frame,
			int is_banner)
{
	/* for now only text rendering is supported */
	return text_render_frame(ctx, frame, is_banner);
}

/* Send a banner showing where a given device starts in the merged stream */
static void flush_banner_frame(struct frame *ref)
{
	int ret;
	char str[128];
	uint64_t stamp;
	struct frame frame;
	struct log_device *dev = ref->dev;

	/* build a fake log entry */
	stamp = ref->stamp;
	snprintf(str, sizeof(str), "------------- beginning of %s", dev->path);
	frame.entry.tv_sec = (time_t)(stamp/1000000ULL);
	frame.entry.tv_nsec = (long)(stamp-frame.entry.tv_sec*1000000ULL)*1000;
	frame.entry.priority = ULOG_INFO;
	frame.entry.pid = getpid();
	frame.entry.pname = "";
	frame.entry.tid = frame.entry.pid;
	frame.entry.tname = frame.entry.pname;
	frame.entry.tag = "ulogcat";
	frame.entry.message = str;
	frame.entry.len = strlen(str)+1;
	frame.entry.is_binary = 0;
	frame.entry.color = 0xffffff;
	frame.dev = dev;

	ret = render_frame(dev->ctx, &frame, 1);
	if (ret == 0)
		output_rendered(dev->ctx);
}

/* Render a frame and output it */
static void flush_frame(struct ulogcat3_context *ctx, struct frame *frame)
{
	int ret;
	struct log_device *dev = frame->dev;

	/* prepend banner if this is the first printed entry for this device */
	if (!dev->printed && (ctx->device_count > 1)) {
		flush_banner_frame(frame);
		dev->printed = 1;
	}

	ret = dev->parse_entry(frame);
	if (ret < 0)
		return;

	ret = render_frame(ctx, frame, 0);
	if (ret == 0)
		output_rendered(ctx);
}

static void enqueue_render(struct ulogcat3_context *ctx, struct frame *frame)
{
	list_add_tail(&ctx->render_queue, &frame->flist);

	if (ctx->render < ctx->tail) {
		ctx->render++;
	} else {
		/* keep queue from growing if we already have enough lines */
		frame = node_to_item(list_head(&ctx->render_queue),
				     struct frame, flist);
		list_remove(&frame->flist);
		free_frame(ctx, frame);
	}
}

static void flush_render_frame(struct ulogcat3_context *ctx, int drop)
{
	struct frame *frame;

	if (ctx->render > 0) {
		frame = node_to_item(list_head(&ctx->render_queue),
				     struct frame, flist);
		if (!drop)
			flush_frame(ctx, frame);
		list_remove(&frame->flist);
		free_frame(ctx, frame);
		ctx->render--;
	}
}

static void flush_render_queue(struct ulogcat3_context *ctx)
{
	while (ctx->render > 0)
		flush_render_frame(ctx, 0);
}

static void drop_render_frame(struct ulogcat3_context *ctx)
{
	flush_render_frame(ctx, 1);
}

static void flush_pending_frame(struct ulogcat3_context *ctx, int drop)
{
	struct frame *frame;

	if (ctx->pending > 0) {
		frame = find_oldest_pending_frame(ctx);
		if (!drop)
			flush_frame(ctx, frame);
		frame->dev->pending = 0;
		list_remove(&frame->flist);
		free_frame(ctx, frame);
		ctx->pending--;
	}
}

static void drop_pending_frame(struct ulogcat3_context *ctx)
{
	flush_pending_frame(ctx, 1);
}

static void flush_pending_queue(struct ulogcat3_context *ctx)
{
	while (ctx->pending > 0)
		flush_pending_frame(ctx, 0);
}

/*
 * Check if we have reached the 'mark', i.e. if we have read for each device
 * at least the amount of data that was readable at initialization.
 */
static void update_mark_reached(struct ulogcat3_context *ctx)
{
	struct listnode *node;
	struct log_device *dev;

	if (ctx->mark_reached)
		return;

	/* have we read the required amount of data from each device ? */
	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);
		if (dev->mark_readable > 0)
			/* not done on this device */
			return;
	}
	ctx->mark_reached = 1;
}

/*
 * Check if we are done buffering in render queue in order to comply with
 * outputting only tailing lines.
 *
 * If we are done, drop entries in excess so that we only have the required
 * number of lines left, and flush.
 */
static void process_tail_flush(struct ulogcat3_context *ctx)
{
	if (!ctx->tail || !ctx->mark_reached)
		return;

	/* drop unwanted frames, we want the exact specified number of lines */
	while (ctx->render + ctx->pending > ctx->tail) {

		/* drop oldest frames first */
		if (ctx->render > 0) {
			drop_render_frame(ctx);
			continue;
		}

		/* drop remaining pending frames */
		if (ctx->pending > 0) {
			drop_pending_frame(ctx);
			continue;
		}
	}

	/* buffering is not required anymore, we can flush everything */
	flush_render_queue(ctx);
	ctx->tail = 0;
}

/*
 * Read entries from active device, and flush one frame.
 * Returns the number of frames read.
 */
static int process_devices(struct ulogcat3_context *ctx, int timeout_ms)
{
	int ret, frames = 0;
	struct frame *frame;
	struct listnode *node;
	struct log_device *dev;

	/* setup descriptors */
	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);
		ctx->fds[dev->idx].fd = dev->pending ? -1 : dev->fd;
	}

	/* force non-blocking wait if we have pending frames */
	if (ctx->pending > 0 || (!ctx->mark_reached && ctx->tail > 0))
		timeout_ms = 0;

	ret = poll(ctx->fds, ctx->device_count, timeout_ms);
	if (ret < 0) {
		if (errno == EINTR)
			return 0;
		INFO("poll: %s\n", strerror(errno));
		return -1;
	}

	/* read one entry per active device and add it to pending queue */
	list_for_each(node, &ctx->log_devices) {

		dev = node_to_item(node, struct log_device, dlist);
		if (!(ctx->fds[dev->idx].revents & POLLIN)) {
			if ((ctx->fds[dev->idx].fd >= 0) &&
			    (dev->mark_readable > 0)) {
				/* we reached the mark for this device */
				dev->mark_readable = 0;
			}
			continue;
		}

		frame = alloc_frame(ctx);
		if (frame == NULL)
			continue;

		ret = dev->receive_entry(dev, frame);
		if (ret <= 0) {
			free_frame(ctx, frame);
			return ret;
		}

		if (ret > 0) {
			list_add_tail(&ctx->pending_queue, &frame->flist);
			dev->pending = 1;
			ctx->pending++;
			frames++;
		}
	}

	/* pull oldest frame from pending queue */
	frame = find_oldest_pending_frame(ctx);
	if (frame) {
		list_remove(&frame->flist);
		frame->dev->pending = 0;
		ctx->pending--;

		if (ctx->tail > 0) {
			/* we only want tailing lines, push to render queue */
			enqueue_render(ctx, frame);
		} else {
			/* no need to queue frame, flush it now */
			flush_frame(ctx, frame);
			free_frame(ctx, frame);
		}
	}

	update_mark_reached(ctx);
	process_tail_flush(ctx);

	return frames;
}

/*
 * Read log entries from devices and output them.
 *
 * Returns -1 if an error occured
 *          0 if no additional work is needed
 *          1 if more work is needed
 */
LIBULOGCAT_API int ulogcat3_process_logs(struct ulogcat3_context *ctx,
					 int max_entries)
{
	int frames = 0, ret = -1, timeout_ms;

	timeout_ms = (ctx->flags & ULOGCAT_FLAG_DUMP) ? 0 : -1;

	do {
		ret = process_devices(ctx, timeout_ms);
		if (ret < 0)
			return ret;
		frames += ret;

		/* in dump mode, stop when mark is reached */
		if ((ctx->flags & ULOGCAT_FLAG_DUMP) && ctx->mark_reached) {
			flush_pending_queue(ctx);
			return 0;
		}

		if (ctx->output_error)
			return -1;

	} while (!max_entries || (frames < max_entries));

	return ret;
}


struct log_device *log_device_create(struct ulogcat3_context *ctx)
{
	struct log_device *dev;

	dev = calloc(1, sizeof(*dev));
	if (dev) {
		dev->ctx = ctx;
		list_add_tail(&ctx->log_devices, &dev->dlist);
		dev->idx = ctx->device_count++;
	} else {
		INFO("cannot allocate device: %s\n", strerror(errno));
	}
	return dev;
}

void log_device_destroy(struct log_device *dev)
{
	if (dev) {
		/* remove from global list */
		list_remove(&dev->dlist);
		dev->ctx->device_count--;

		if (dev->fd >= 0) {
			close(dev->fd);
			dev->fd = -1;
		}

		free(dev->priv);
		free(dev);
	}
}

LIBULOGCAT_API struct ulogcat3_context *
ulogcat3_open(const struct ulogcat_opts_v3 *opts, const char **devices,
	      int ndevices)
{
	int ret, i, nframes;
	struct listnode *node;
	struct log_device *dev;
	struct ulogcat3_context *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		goto fail;

	ctx->log_format = opts->opt_format;
	ctx->flags = opts->opt_flags;
	ctx->tail = opts->opt_tail;
	ctx->output_fd = opts->opt_output_fd;
	ctx->output_fp = opts->opt_output_fp;

	/* default output is stdout */
	if ((ctx->output_fd < 0) && (ctx->output_fp == NULL))
		ctx->output_fp = stdout;

	if (ctx->output_fp)
		/* we want a line-buffered output */
		setlinebuf(ctx->output_fp);

	list_init(&ctx->log_devices);
	list_init(&ctx->free_queue);
	list_init(&ctx->render_queue);
	list_init(&ctx->pending_queue);

	if (ctx->flags & ULOGCAT_FLAG_COLOR)
		setup_colors(ctx);

	/* add user specified buffers */
	for (i = 0; i < ndevices; i++) {
		/* skip special kmsgd buffer */
		if (strcmp(devices[i], KMSGD_ULOG_NAME) != 0) {
			ret = add_ulog_device(ctx, devices[i]);
			if (ret)
				goto fail;
		}
	}

	/* automatically add all ulog devices if none were explicitly given */
	if (!ctx->ulog_device_count && (ctx->flags & ULOGCAT_FLAG_ULOG)) {
		ret = add_all_ulog_devices(ctx);
		if (ret)
			goto fail;
	}

	/* kernel device */
	if (ctx->flags & ULOGCAT_FLAG_KLOG) {
		/* on recent kernels, read records directly from /dev/kmsg */
		ret = add_klog_device(ctx);
		if (ret) {
			/*
			 * On older kernels, read messages from /dev/ulog_kmsgd,
			 * assuming kmsgd daemon copies kernel messages to that
			 * ulog device.
			 */
			ret = add_ulog_device(ctx, KMSGD_ULOG_NAME);
			if (ret)
				goto fail;
		}
	}

	/* we want at least one device */
	if (ctx->device_count == 0) {
		INFO("could not open any device\n");
		goto fail;
	}

	/* setup file descriptors */
	ctx->fds = malloc(ctx->device_count*sizeof(*ctx->fds));
	if (ctx->fds == NULL)
		goto fail;

	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);
		ctx->fds[dev->idx].events = POLLIN;
	}

	/* setup rendering buffer */
	ctx->render_size = text_render_size();
	ctx->render_buf = malloc(ctx->render_size);
	if (ctx->render_buf == NULL)
		goto fail;

	/* setup frame pool: allocate enough for pending and render queues */
	nframes = ctx->tail + ctx->device_count + 1;
	ctx->frame_pool = calloc(1, nframes*sizeof(*ctx->frame_pool));
	if (ctx->frame_pool == NULL)
		goto fail;

	for (i = 0; i < nframes; i++) {
		ctx->frame_pool[i].buf = ctx->frame_pool[i].data;
		ctx->frame_pool[i].bufsize = sizeof(ctx->frame_pool[i].data);
		list_add_tail(&ctx->free_queue, &ctx->frame_pool[i].flist);
	}

	return ctx;

fail:
	ulogcat3_close(ctx);
	return NULL;
}

LIBULOGCAT_API void ulogcat3_close(struct ulogcat3_context *ctx)
{
	struct frame *f;
	struct log_device *dev;

	if (ctx) {
		/* close and destroy devices */
		while (!list_empty(&ctx->log_devices)) {
			dev = node_to_item(list_head(&ctx->log_devices),
					   struct log_device, dlist);
			list_remove(&dev->dlist);
			log_device_destroy(dev);
		}
		ctx->device_count = 0;

		/* destroy queues */
		while (!list_empty(&ctx->render_queue)) {
			f = node_to_item(list_head(&ctx->render_queue),
					 struct frame, flist);
			list_remove(&f->flist);
			free_frame(ctx, f);
		}
		while (!list_empty(&ctx->pending_queue)) {
			f = node_to_item(list_head(&ctx->pending_queue),
					 struct frame, flist);
			list_remove(&f->flist);
			free_frame(ctx, f);
		}

		/* close descriptors */
		if (ctx->output_fd >= 0)
			close(ctx->output_fd);

		if (ctx->output_fp)
			fclose(ctx->output_fp);

		free(ctx->frame_pool);
		free(ctx->render_buf);
		free(ctx->fds);
		free(ctx);
	}
}

LIBULOGCAT_API int ulogcat3_clear(struct ulogcat3_context *ctx)
{
	int ret = 0;
	struct listnode *node;
	struct log_device *dev;

	if (ctx) {
		list_for_each(node, &ctx->log_devices) {
			dev = node_to_item(node, struct log_device, dlist);
			ret = dev->clear_buffer(dev);
			if (ret < 0)
				break;
		}
	}

	return ret;
}
