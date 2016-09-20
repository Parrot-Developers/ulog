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

#include "libulogcat_private.h"

void set_error(struct ulogcat_context *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(ctx->last_error, sizeof(ctx->last_error), fmt, ap);
	va_end(ap);
}

static void output_frame(struct ulogcat_context *ctx, struct frame *frame)
{
	ssize_t ret;

	if (ctx->output_handler) {
		/* custom output handler */
		ctx->output_handler(ctx->output_handler_data, frame->buf,
				    frame->size);
	} else {
		/* just use output file descriptor */
		do {
			ret = write(ctx->output_fd, frame->buf, frame->size);
		} while ((ret < 0) && (errno == EINTR));
	}
}

static struct frame *alloc_frame(struct ulogcat_context *ctx)
{
	unsigned int framesize;
	struct frame *f = NULL;

	if (!list_empty(&ctx->free_frames)) {
		/* reuse frame */
		f = node_to_item(list_head(&ctx->free_frames), struct frame,
				 flist);
		list_remove(&f->flist);
	} else {
		/* no frames left */
		framesize = sizeof(struct frame) + ctx->render_frame_size;
		f = malloc(framesize);
	}
	return f;
}

static void free_frame(struct ulogcat_context *ctx, struct frame *frame)
{
	if (frame)
		list_add_tail(&ctx->free_frames, &frame->flist);
}

static void enqueue_frame(struct log_device *dev, struct frame *frame)
{
	struct listnode *node;
	struct frame *item;

	/* queue is sorted in increasing stamp order */
	list_for_each_reverse(node, &dev->queue) {
		item = node_to_item(node, struct frame, flist);
		if (frame->stamp >= item->stamp)
			/* we found a smaller stamp */
			break;
	}

	list_add_tail(list_head(node), &frame->flist);
	dev->ctx->queued_frames++;
	dev->pending_frames++;

	DEBUG("core: enqueued frame from %s (pending=%d global=%d)\n",
	      dev->path, dev->pending_frames, dev->ctx->queued_frames);
}

static struct log_device *
find_device_with_oldest_frame(struct ulogcat_context *ctx, int *lastframe)
{
	struct listnode *node;
	struct frame *item;
	struct log_device *dev, *res = NULL;
	uint64_t stamp = 0ULL;

	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);
		if (list_empty(&dev->queue))
			continue;
		item = node_to_item(list_head(&dev->queue), struct frame,
				    flist);
		if (!res || (item->stamp < stamp)) {
			res = dev;
			stamp = item->stamp;
			/* is this the last frame in queue ? */
			if (lastframe) {
				if (list_tail(&dev->queue) == &item->flist)
					*lastframe = 1;
				else
					*lastframe = 0;
			}
		}
	}

	if (res)
		DEBUG("core: oldest frame from %s (%s) stamp=%llu last=%d\n",
		      res->path,
		      (res->state == ACTIVE) ? "ACTIVE" :
		      ((res->state == PAUSED) ? "PAUSED" : "IDLE"),
		      item->stamp, lastframe ? *lastframe : 0);
	return res;
}

static int render_frame(struct ulogcat_context *ctx,
			const struct log_entry *_entry, struct frame *frame,
			int is_banner)
{
	const struct ulog_entry *entry = &_entry->ulog;

	if (frame == NULL)
		return -1;

	frame->stamp = entry->tv_sec*1000000ULL + entry->tv_nsec/1000ULL;
	frame->is_banner = is_banner;

	return ctx->render_frame(ctx, _entry, frame);
}

/* Send a banner showing where a given device starts in the merged stream */
static void output_banner_frame(struct log_device *dev, uint64_t stamp)
{
	char str[128];
	struct frame *frame;
	struct log_entry entry;

	snprintf(str, sizeof(str), "------------- beginning of %s", dev->path);

	/* build a fake log entry */
	entry.ulog.tv_sec = (time_t)(stamp/1000000ULL);
	entry.ulog.tv_nsec = (long)(stamp-entry.ulog.tv_sec*1000000ULL)*1000;
	entry.ulog.priority = ULOG_INFO;
	entry.ulog.pid = getpid();
	entry.ulog.pname = "";
	entry.ulog.tid = entry.ulog.pid;
	entry.ulog.tname = entry.ulog.pname;
	entry.ulog.tag = "ulogcat";
	entry.ulog.message = str;
	entry.ulog.len = strlen(str)+1;
	entry.ulog.is_binary = 0;
	entry.ulog.color = 0xffffff;
	entry.label = dev->label;

	frame = alloc_frame(dev->ctx);

	if (render_frame(dev->ctx, &entry, frame, 1) == 0)
		output_frame(dev->ctx, frame);

	free_frame(dev->ctx, frame);
}

static void flush_frame(struct log_device *dev, struct frame *frame)
{
	struct ulogcat_context *ctx = dev->ctx;

	if (!dev->printed && (ctx->device_count > 1)) {
		output_banner_frame(dev, frame->stamp);
		dev->printed = 1;
	}
	output_frame(ctx, frame);

	list_remove(&frame->flist);
	free_frame(ctx, frame);
	ctx->queued_frames--;
	dev->pending_frames--;
}

static struct log_device *flush_one_frame(struct ulogcat_context *ctx)
{
	struct log_device *dev;
	struct frame *f;

	dev = find_device_with_oldest_frame(ctx, NULL);
	if (dev) {
		f = node_to_item(list_head(&dev->queue), struct frame, flist);
		flush_frame(dev, f);
	}
	return dev;
}

static void flush_all_frames(struct ulogcat_context *ctx)
{
	struct log_device *dev;

	DEBUG("core: flushing all frames\n");

	do {
		dev = flush_one_frame(ctx);
	} while (dev);
}

static void flush_all_frames_until_single_frame(struct ulogcat_context *ctx)
{
	struct frame *frame;
	struct log_device *dev;
	int lastframe, flushable;

	do {
		dev = find_device_with_oldest_frame(ctx, &lastframe);
		if (dev == NULL)
			break;

		frame = node_to_item(list_head(&dev->queue), struct frame,
				     flist);

		/* can we flush this frame ? */
		flushable = !lastframe || (dev->state == IDLE);
		if (flushable)
			flush_frame(dev, frame);

	} while (flushable);
}

static int receive_entry(struct log_device *dev, struct log_entry *entry)
{
	entry->label = dev->label;
	return dev->receive_entry(dev, &entry->ulog);
}

static int process_devices(struct ulogcat_context *ctx, int force)
{
	int ret, frames = 0;
	struct frame *frame;
	struct listnode *node;
	struct log_device *dev;
	struct log_entry entry;

	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);

		if ((ctx->fds[dev->idx].fd < 0) && !force)
			/* paused device */
			continue;

		if (!(ctx->fds[dev->idx].revents & POLLIN) && !force) {
			dev->state = IDLE;
			DEBUG("core: %s -> IDLE\n", dev->path);
			continue;
		}

		DEBUG("core: %sdata on %s\n", force ? "force " : "", dev->path);

		do {
			/* read as many frames as we can */
			ret = receive_entry(dev, &entry);
			if (ret < 0)
				return -1;
			if (ret > 0) {
				entry.label = dev->label;
				frame = alloc_frame(ctx);
				if (render_frame(ctx, &entry, frame, 0) == 0)
					enqueue_frame(dev, frame);
				else
					free_frame(ctx, frame);
				frames++;
			}
		} while (ret && (dev->pending_frames < MAX_PENDING_FRAMES));
	}

	return frames;
}

struct log_device *log_device_create(struct ulogcat_context *ctx)
{
	struct log_device *dev;

	dev = calloc(1, sizeof(*dev));
	if (dev) {
		dev->ctx = ctx;
		list_init(&dev->queue);
		list_add_tail(&ctx->log_devices, &dev->dlist);
		dev->idx = ctx->device_count++;
	} else {
		set_error(ctx, "cannot allocate device: %s", strerror(errno));
	}
	return dev;
}

void log_device_destroy(struct log_device *dev)
{
	struct frame *f;

	if (dev) {
		/* remove from global list */
		list_remove(&dev->dlist);
		dev->ctx->device_count--;

		/* destroy frame queue */
		while (!list_empty(&dev->queue)) {
			f = node_to_item(list_head(&dev->queue), struct frame,
					 flist);
			list_remove(&f->flist);
			free(f);
		}

		if (dev->fd >= 0) {
			close(dev->fd);
			dev->fd = -1;
		}

		free(dev->priv);
		free(dev);
	}
}

LIBULOGCAT_API int ulogcat_init(struct ulogcat_context *ctx)
{
	int ret;

	/* automatically add all ulog devices if none were explicitly given */
	if (!ctx->ulog_device_count && (ctx->flags & ULOGCAT_FLAG_ULOG)) {
		ret = add_all_ulog_devices(ctx);
		if (ret)
			return -1;
	}

	/* automatically add all alog devices if none were explicitly given */
	if (!ctx->alog_device_count && (ctx->flags & ULOGCAT_FLAG_ALOG)) {
		ret = add_all_alog_devices(ctx);
		if (ret)
			return -1;
	}

	/* kernel device */
	if (ctx->flags & ULOGCAT_FLAG_KLOG) {
		/* kernel messages are now retrieved from a ulog device */
		ret = add_ulog_device(ctx, KMSGD_ULOG_NAME);
		if (ret)
			DEBUG("cannot get kernel messages: %s\n",
			      ulogcat_strerror(ctx));
	}

	/* we want at least one device */
	if (ctx->device_count == 0) {
		set_error(ctx, "could not open any device");
		return -1;
	}

	return 0;
}

static int ulogcat_do_ioctl(struct ulogcat_context *ctx)
{
	struct listnode *node;
	struct log_device *dev;
	int size, readable, ret = 0;

	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);

		if (ctx->flags & ULOGCAT_FLAG_CLEAR) {
			ret = dev->clear_buffer(dev);
			if (ret < 0)
				break;
		}

		if (ctx->flags & ULOGCAT_FLAG_GET_SIZE) {

			ret = dev->get_size(dev, &size, &readable);
			if (ret < 0)
				break;

			printf("%s: ring buffer is %dKb (%dKb consumed)\n",
			       dev->path, size / 1024, readable / 1024);
		}
	}

	return ret;
}

static void prepare_poll(struct ulogcat_context *ctx)
{
	struct listnode *node;
	struct log_device *dev;

	/* pause devices for which we have enough pending frames */
	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);
		if (dev->pending_frames < MAX_PENDING_FRAMES) {
			ctx->fds[dev->idx].fd = dev->fd;
			dev->state = ACTIVE;
		} else {
			ctx->fds[dev->idx].fd = -1;
			DEBUG("core: %s -> PAUSED\n", dev->path);
			dev->state = PAUSED;
		}
	}
}

static int is_partial_poll(struct ulogcat_context *ctx)
{
	struct listnode *node;
	struct log_device *dev;

	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);
		if (ctx->fds[dev->idx].fd == -1)
			return 1;
	}
	return 0;
}

LIBULOGCAT_API int ulogcat_process_descriptors(struct ulogcat_context *ctx,
					       int poll_result, int *timeout_ms)
{
	DEBUG("core: poll(%d) --> %d\n", *timeout_ms, poll_result);

	if (process_devices(ctx, 0) < 0)
		return -1;

	if ((poll_result == 0) && !is_partial_poll(ctx)) {
		/* we have been idle long enough, flush everything */
		flush_all_frames(ctx);
		if (ctx->flags & ULOGCAT_FLAG_DUMP)
			/* we are done */
			return 1;
		/* now that we are idle, we may sleep indefinitely */
		*timeout_ms = -1;
	} else {
		/*
		 * We are still busy; flush as much as we can, but stop
		 * when a single frame is left in a queue, in case a
		 * burst on that queue is coming.
		 */
		flush_all_frames_until_single_frame(ctx);
		*timeout_ms = FRAME_IDLE_TIMEOUT_MS;
	}

	/* we need to continue */
	prepare_poll(ctx);
	return 0;
}

LIBULOGCAT_API void ulogcat_flush_descriptors(struct ulogcat_context *ctx)
{
	int frames, total_frames = 0;

	do {
		/* force processing of all devices */
		frames = process_devices(ctx, 1);
		if (frames > 0) {
			flush_all_frames(ctx);
			total_frames += frames;
		}
	} while ((frames > 0) && (total_frames < MAX_FLUSHED_FRAMES));
}

LIBULOGCAT_API int ulogcat_get_descriptor_nb(struct ulogcat_context *ctx)
{
	return ctx->device_count;
}

LIBULOGCAT_API int ulogcat_setup_descriptors(struct ulogcat_context *ctx,
					     struct pollfd *fds, int nfds)
{
	struct listnode *node;
	struct log_device *dev;

	if (nfds < ctx->device_count)
		return -1;

	ctx->fds = fds;

	/* device descriptors */
	list_for_each(node, &ctx->log_devices) {
		dev = node_to_item(node, struct log_device, dlist);
		ctx->fds[dev->idx].fd = dev->fd;
		ctx->fds[dev->idx].events = POLLIN;
		dev->state = ACTIVE;
	}

	prepare_poll(ctx);
	return 0;
}

static int ulogcat_read_log_lines(struct ulogcat_context *ctx)
{
	int ret = -1, timeout_ms;
	struct pollfd *fds;
	int nfds;

	timeout_ms = (ctx->flags & ULOGCAT_FLAG_DUMP) ? 0 : -1;
	nfds = ulogcat_get_descriptor_nb(ctx);

	fds = malloc(nfds*sizeof(*fds));
	if (fds == NULL)
		goto finish;

	ret = ulogcat_setup_descriptors(ctx, fds, nfds);
	if (ret < 0)
		goto finish;

	while (1) {

		ret = poll(fds, nfds, timeout_ms);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			set_error(ctx, "poll: %s", strerror(errno));
			break;
		}

		ret = ulogcat_process_descriptors(ctx, ret, &timeout_ms);
		if (ret == 1) {
			ret = 0;
			break;
		} else if (ret < 0) {
			break;
		}
	}
finish:
	free(fds);
	return ret;
}

LIBULOGCAT_API struct ulogcat_context *
ulogcat_create2(const struct ulogcat_opts_v2 *opts)
{
	struct ulogcat_context *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		goto fail;

	ctx->log_format = opts->opt_format;

	switch (ctx->log_format) {
	case ULOGCAT_FORMAT_CKCM:
		ctx->render_frame = render_ckcm_frame;
		ctx->render_frame_size = ckcm_frame_size();
		break;
	case ULOGCAT_FORMAT_BINARY:
		ctx->render_frame = render_binary_frame;
		ctx->render_frame_size = binary_frame_size();
		break;
	default:
		ctx->render_frame = render_text_frame;
		ctx->render_frame_size = text_frame_size();
		break;
	}

	ctx->flags = opts->opt_flags;

	ctx->output_fd = opts->opt_output_fd;
	if (ctx->output_fd < 0)
		ctx->output_fd = STDOUT_FILENO;
	ctx->output_handler = opts->opt_output_handler;
	ctx->output_handler_data = opts->opt_output_handler_data;

	list_init(&ctx->log_devices);
	list_init(&ctx->free_frames);

	if (ctx->flags & ULOGCAT_FLAG_COLOR)
		setup_colors(ctx);

	DEBUG("core: created context %p\n", ctx);
	return ctx;
fail:
	ulogcat_destroy(ctx);
	return NULL;
}

/* v1 interface */
LIBULOGCAT_API struct ulogcat_context *
ulogcat_create(const struct ulogcat_opts *opts)
{
	struct ulogcat_opts_v2 opts_v2;

	memset(&opts_v2, 0, sizeof(opts_v2));

	/* emulate v2 format */
	opts_v2.opt_format = opts->opt_format;
	if (opts->opt_binary)
		opts_v2.opt_format = ULOGCAT_FORMAT_BINARY;

	opts_v2.opt_flags = (ULOGCAT_FLAG_ULOG                              |
			     (opts->opt_clear   ? ULOGCAT_FLAG_CLEAR    : 0)|
			     (opts->opt_dump    ? ULOGCAT_FLAG_DUMP     : 0)|
			     (opts->opt_color   ? ULOGCAT_FLAG_COLOR    : 0)|
			     (opts->opt_getsize ? ULOGCAT_FLAG_GET_SIZE : 0));

	/* tail and rotating options are not supported anymore */
	opts_v2.opt_output_fd = opts->output_fd;

	return ulogcat_create2(&opts_v2);
}

LIBULOGCAT_API void ulogcat_destroy(struct ulogcat_context *ctx)
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

		/* destroy free frames */
		while (!list_empty(&ctx->free_frames)) {
			f = node_to_item(list_head(&ctx->free_frames),
					 struct frame, flist);
			list_remove(&f->flist);
			free(f);
		}

		/* close descriptors */
		if (ctx->output_fd >= 0)
			close(ctx->output_fd);

		DEBUG("core: destroyed context %p\n", ctx);

		free(ctx);
	}
}

LIBULOGCAT_API const char *ulogcat_strerror(struct ulogcat_context *ctx)
{
	return ctx->last_error;
}

LIBULOGCAT_API int ulogcat_add_device(struct ulogcat_context *ctx,
				      const char *name)
{
	int ret = 0;

	/* skip special kmsgd buffer */
	if (strcmp(name, KMSGD_ULOG_NAME) != 0)
		ret = add_ulog_device(ctx, name);

	return ret;
}

LIBULOGCAT_API int ulogcat_add_android_device(struct ulogcat_context *ctx,
					      const char *name)
{
	return add_alog_device(ctx, name);
}

LIBULOGCAT_API int ulogcat_process_logs(struct ulogcat_context *ctx)
{
	int ret;

	ret = ulogcat_init(ctx);
	if (ret < 0)
		return ret;

	if (ctx->flags & (ULOGCAT_FLAG_GET_SIZE|ULOGCAT_FLAG_CLEAR))
		ret = ulogcat_do_ioctl(ctx);
	else
		ret = ulogcat_read_log_lines(ctx);

	return ret;
}
