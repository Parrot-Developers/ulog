/**
 * Copyright (C) 2017 Parrot Drones.
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <libpomp.h>
#include <getopt.h>
#include <futils/timetools.h>
#include <ulograw.h>
#include <ulog_shd.h>
#define SHD_ADVANCED_READ_API
#include <libshdata.h>

#define ULOG_TAG shdlogd
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#define SHDLOGD_DEFAULT_PERIOD_MS 50
#define SHDLOGD_DEFAULT_SECTION_NAME "ulog"
#define SHDLOGD_DEFAULT_DEVICE_NAME NULL
#define SHDLOGD_DEFAULT_PROCESS_NAME "rtos"
#define SHDLOGD_DEFAULT_PID 0

static struct {
	bool stop;
	uint32_t period_ms;
	char *section;
	char *device;
	unsigned short int index;
	struct pomp_loop *loop;
	struct pomp_timer *timer;
	int ulogfd;
	struct ulog_raw_entry raw;
	struct {
		struct shd_ctx *ctx;
		struct shd_revision *rev;
		struct shd_sample_search search;
		size_t blobs_size;
		struct ulog_shd_blob *blobs;
		struct timespec *ts;
	} shd;
} ctx = {
	.stop = false,
	.period_ms = SHDLOGD_DEFAULT_PERIOD_MS,
	.section = SHDLOGD_DEFAULT_SECTION_NAME,
	.device = SHDLOGD_DEFAULT_DEVICE_NAME,
	.index = 0,
	.loop = NULL,
	.timer = NULL,
	.ulogfd = -1,
	.raw = {
		.entry = {
			.pid = SHDLOGD_DEFAULT_PID,
			.tid = SHDLOGD_DEFAULT_PID,
		},
		.pname = SHDLOGD_DEFAULT_PROCESS_NAME,
		.pname_len = sizeof(SHDLOGD_DEFAULT_PROCESS_NAME),
	},
	.shd = {
		.ctx = NULL,
		.rev = NULL,
		.search = {
			.method = SHD_OLDEST,
			.nb_values_before_date = 0,
			.nb_values_after_date = ULOG_SHD_NB_SAMPLES - 1
		},
		.blobs_size = sizeof(struct ulog_shd_blob)
			* ULOG_SHD_NB_SAMPLES,
		.blobs = NULL,
		.ts = NULL,
	}
};

static const uint32_t shdcolor[] = {
	0x000000,	/* black	*/
	0xFF0000,	/* red		*/
	0x00FF00,	/* green	*/
	0xFFFF00,	/* yellow	*/
	0x0000FF,	/* blue		*/
	0xFF00FF,	/* magenta	*/
	0xFFFF00,	/* cyan		*/
	0x808080,	/* gray		*/
};

static void fill_raw_entry(struct ulog_raw_entry *raw,
				struct ulog_shd_blob *blob,
				struct timespec *ts)
{
	if (blob->thnsize)
		raw->entry.tid = blob->tid;
	else
		raw->entry.tid = SHDLOGD_DEFAULT_PID;
	raw->entry.sec = ts->tv_sec;
	raw->entry.nsec = ts->tv_nsec;

	raw->prio = blob->prio;
	raw->tname = blob->buf;
	raw->tag = raw->tname + blob->thnsize;
	raw->message = raw->tag + blob->tagsize;
	raw->tname_len = blob->thnsize;
	raw->tag_len = blob->tagsize;
	raw->message_len = blob->logsize;

	/* Some log messages may start with an escape character to add
	 * a color information in the form '\033[0;3#m'.
	 * 7 characters are then removed at the beginning of the log and
	 * character log[5] is used to identify the color (between 0
	 * and 7) according to the array shdcolor. */
	if (raw->message[0] == '\033' && (raw->message_len >= 7)) {
		raw->prio |= shdcolor[(raw->message[5] - 0x30) & 0x7]
						<< ULOG_PRIO_COLOR_SHIFT;
		raw->message += 7;
		raw->message_len -= 7;
	}
}

static int read_samples()
{
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	int ret, i;
	short int d;

	ret = shd_select_samples(ctx.shd.ctx, &ctx.shd.search, &metadata,
					&result);
	if (ret < 0) {
		if (ret != -ENOENT && ret != -EAGAIN)
			ULOGE("shd_select_samples failed: %s", strerror(-ret));
		return ret;
	}

	/* Save timestamps */
	for (i = 0; i < result.nb_matches; i++)
		ctx.shd.ts[i] = metadata[i].ts;

	/* Read samples */
	ret = shd_read_quantity(ctx.shd.ctx, NULL, ctx.shd.blobs,
				ctx.shd.blobs_size);
	if (ret < 0)
		ULOGE("shd read samples failed: %s", strerror(-ret));

	ret = shd_end_read(ctx.shd.ctx, ctx.shd.rev);
	if (ret < 0) {
		ULOGE("shd end_read failed: %s", strerror(-ret));
		if (ret == -ENODEV)
			ctx.stop = true;
	}

	if (ret < 0)
		return ret;

	/* Send samples to ulog */
	for (i = 0; i < result.nb_matches; i++) {

		fill_raw_entry(&ctx.raw, &ctx.shd.blobs[i], &ctx.shd.ts[i]);
		ret = ulog_raw_log(ctx.ulogfd, &ctx.raw);

		/* check index vs previous index */
		d = ctx.shd.blobs[i].index - ctx.index - 1;
		if (d > 0)
			ULOGE("%d shared memory log messages lost", d);
		else if (d < 0)
			ULOGE("many shared memory log messages lost");

		ctx.index = ctx.shd.blobs[i].index;
	}

	/* add 1ns to the last received sample timestamp to get the next ones */
	time_timespec_add_ns(&ctx.shd.ts[result.nb_matches - 1], 1,
							&ctx.shd.search.date);

	return 0;
}

static void on_timer(struct pomp_timer *timer, void *userdata)
{
	read_samples();
}

static void on_signal(int signum)
{
	ctx.stop = true;
	ULOGI("signal %d (%s) received", signum, strsignal(signum));
}

static void usage(void)
{
	printf("usage: shdlogd [-h] [-p PERIOD] [-s NAME] [-d NAME]\n"
		"Retrieve logs from the shared memory and log them with ulog.\n"
		"\n"
		"  -h, --help           print this help message\n"
		"  -p, --period  PERIOD polling period in milliseconds (default %dms)\n"
		"  -s, --section NAME   name of the section in shared memory (default %s)\n"
		"  -d, --device  NAME   name of the ulogger device\n"
		"  -n, --pname  NAME   name of the process in ulog (default %s)\n"
		"\n", SHDLOGD_DEFAULT_PERIOD_MS, SHDLOGD_DEFAULT_SECTION_NAME,
		SHDLOGD_DEFAULT_PROCESS_NAME);
}

static bool parse_opts(int argc, char *argv[])
{
	bool  run = true;

	while (1) {
		static const struct option lopts[] = {
			{ "period",  1, 0, 'p' },
			{ "section", 1, 0, 's' },
			{ "device",  1, 0, 'd' },
			{ "pname",   1, 0, 'n' },
			{ "help",    0, 0, 'h' },
			{ NULL, 0, 0, 0 }
		};
		int c;

		c = getopt_long(argc, argv, "s:p:d:n:h", lopts, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 's':
			ctx.section = optarg;
			break;
		case 'd':
			ctx.device = optarg;
			break;
		case 'n':
			ctx.raw.pname = optarg;
			ctx.raw.pname_len = strlen(optarg) + 1;
			break;
		case 'p':
			ctx.period_ms = atoi(optarg);
			break;
		default:
			usage();
			run = false;
			break;
		}
	}

	return run;
}

int main(int argc, char **argv)
{
	int ret = -EINVAL;

	if (!parse_opts(argc, argv))
		return EXIT_SUCCESS;

	ULOGN("shdlogd starting, polling every %" PRIu32 " ms", ctx.period_ms);

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	ctx.loop = pomp_loop_new();
	if (!ctx.loop) {
		ULOGE("can't create pomp loop");
		ret = -EINVAL;
		goto finish;
	}

	ctx.timer = pomp_timer_new(ctx.loop, on_timer, &ctx);
	if (!ctx.timer) {
		ULOGE("can't create pomp timer");
		ret = -EINVAL;
		goto finish;
	}
	ret = pomp_timer_set_periodic(ctx.timer, 1, ctx.period_ms);
	if (ret < 0) {
		ULOGE("can't configure pomp timer: %s", strerror(-ret));
		goto finish;
	}

	ret = ulog_raw_open(ctx.device);
	if (ret < 0) {
		ULOGE("can't open ulogger device \"%s\" in raw mode: %s",
				ctx.device, strerror(-ret));
		goto finish;
	}
	ctx.ulogfd = ret;

	ctx.shd.ctx = shd_open(ctx.section, NULL, &ctx.shd.rev);
	if (!ctx.shd.ctx) {
		ULOGE("can't open shdata context for section %s", ctx.section);
		ret = -EINVAL;
		goto finish;
	}

	ctx.shd.blobs = malloc(ctx.shd.blobs_size);
	if (!ctx.shd.blobs) {
		ULOGE("can't allocate memory for blobs");
		ret = -ENOMEM;
		goto finish;
	}

	ctx.shd.ts =
		malloc(sizeof(struct timespec) * ULOG_SHD_NB_SAMPLES);
	if (!ctx.shd.ts) {
		ULOGE("can't allocate memory for timespecs");
		ret = -ENOMEM;
		goto finish;
	}

	/* Read oldest sample to get a timestamp reference */
	do {
		ret = read_samples();
		if (ctx.stop)
			goto finish;
		usleep(1000);
	} while (ret);

	/* change search method */
	ctx.shd.search.method = SHD_FIRST_AFTER;

	while (!ctx.stop) {
		ret = pomp_loop_wait_and_process(ctx.loop, -1);

		if (ret < 0) {
			ULOGE("pomp loop error: %s", strerror(-ret));
			continue;
		}
	}

finish:
	free(ctx.shd.ts);
	free(ctx.shd.blobs);

	if (ctx.shd.ctx)
		shd_close(ctx.shd.ctx, ctx.shd.rev);
	if (ctx.ulogfd >= 0)
		ulog_raw_close(ctx.ulogfd);
	if (ctx.timer)
		pomp_timer_destroy(ctx.timer);
	if (ctx.loop)
		pomp_loop_destroy(ctx.loop);

	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
