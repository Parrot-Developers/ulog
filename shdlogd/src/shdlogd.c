#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <libpomp.h>
#include <getopt.h>
#define SHD_ADVANCED_READ_API
#include <libshdata.h>
#include <ulog_shd.h>
#include <futils/timetools.h>

#define ULOG_TAG shdlogd
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#define SHDLOGD_DEFAULT_PERIOD_MS 50
#define SHDLOGD_DEFAULT_SECTION_NAME "ulog"

struct {
	bool stop;
	uint32_t period_ms;
	char *section;
	unsigned short int index;
	struct pomp_loop *loop;
	struct pomp_timer *timer;
	struct ulog_cookie cookie;
	struct {
		struct shd_ctx *ctx;
		struct shd_revision *rev;
		struct shd_sample_search search;
	} shd;
} ctx = {
	.stop = false,
	.period_ms = SHDLOGD_DEFAULT_PERIOD_MS,
	.section = SHDLOGD_DEFAULT_SECTION_NAME,
	.index = 0,
	.loop = NULL,
	.timer = NULL,
	.cookie = {
		.level = ULOG_DEBUG
	},
	.shd = {
		.ctx = NULL,
		.rev = NULL,
		.search = {
			.method = SHD_OLDEST,
			.nb_values_before_date = 0,
			.nb_values_after_date = ULOG_SHD_NB_SAMPLES - 1
		}
	}
};

const uint32_t shdcolor[] = {
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
		raw->entry.tid = SHDLOGD_THREADX_DEFAULT_PID;
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
	struct ulog_shd_blob blobs[ULOG_SHD_NB_SAMPLES];
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	int ret, i, l;
	uint32_t prio;
	short int d;
	char *p;

	ret = shd_select_samples(ctx.shd.ctx, &ctx.shd.search, &metadata,
					&result);
	if (ret < 0) {
		if (ret != -ENOENT && ret != -EAGAIN) {
			ULOGE("shd_select_samples failed: %s", strerror(-ret));
			ctx.stop = true;
		}
		return ret;
	}

	/* Read samples */
	ret = shd_read_quantity(ctx.shd.ctx, NULL, blobs, sizeof(blobs));
	if (ret < 0) {
		ULOGE("shd read samples failed: %s", strerror(-ret));
		if (shd_end_read(ctx.shd.ctx, ctx.shd.rev) == -ENODEV)
			ctx.stop = true;
		return ret;
	}

	/* add 1 ns to the current sample timestamp to get the next ones */
	time_timespec_add_ns(&metadata[result.nb_matches - 1].ts, 1,
							&ctx.shd.search.date);

	ret = shd_end_read(ctx.shd.ctx, ctx.shd.rev);
	if (ret < 0) {
		ULOGE("shd end_read failed: %s", strerror(-ret));
		if (ret == -ENODEV)
			ctx.stop = true;
		return ret;
	}

	/* Send samples to ulog */
	for (i = 0; i < result.nb_matches; i++) {
		ctx.cookie.name = blobs[i].tag;
		ctx.cookie.namesize = blobs[i].tagsize;

		/* Some logs may start with an escape character to add
		 * a color information in the form '\033[0;3#m'.
		 * 7 characters are then removed from the log and
		 * character log[5] is used to identify the color (between 0
		 * and 7) according to the array shdcolor. */
		if (blobs[i].log[0] == '\033' && (blobs[i].logsize >= 7)) {
			p = blobs[i].log + 7;
			l = blobs[i].logsize - 7;
			prio = blobs[i].prio |
				(shdcolor[(blobs[i].log[5] - 0x30) & 0x7] <<
					ULOG_PRIO_COLOR_SHIFT);
		} else {
			p = blobs[i].log;
			l = blobs[i].logsize;
			prio = blobs[i].prio;
		}

		ulog_log_buf(prio, &ctx.cookie, p, l);

		/* check index vs previous index */
		d = blobs[i].index - ctx.index;
		if (d != 1)
			ULOGE("%d shared memory log messages lost", d - 1);
		ctx.index = blobs[i].index;
	}

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
	printf("usage: shdlogd [-h] [-p PERIOD] [-s NAME]\n"
		"Retrieve logs from the shared memory and log them with ulog.\n"
		"\n"
		"  -h, --help           print this help message\n"
		"  -p, --period  PERIOD polling period in milliseconds (default %dms)\n"
		"  -s, --section NAME   name of the section in shared memory (default %s)\n"
		"\n", SHDLOGD_DEFAULT_PERIOD_MS, SHDLOGD_DEFAULT_SECTION_NAME);

	return EXIT_FAILURE;
}

static bool parse_opts(int argc, char *argv[])
{
	bool  run = true;

	while (1) {
		static const struct option lopts[] = {
			{ "period",  1, 0, 'p' },
			{ "section", 1, 0, 's' },
			{ "help",    0, 0, 'h' },
			{ NULL, 0, 0, 0 }
		};
		int c;

		c = getopt_long(argc, argv, "s:p:h", lopts, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 's':
			ctx.section = optarg;
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

	ctx.shd.ctx = shd_open(ctx.section, NULL, &ctx.shd.rev);
	if (!ctx.shd.ctx) {
		ULOGE("can't open shdata context for section %s", ctx.section);
		ret = -EINVAL;
		goto finish;
	}

	/* Read oldest sample to get a timestamp reference */
	ret = read_samples();
	if (ret < 0)
		goto finish;

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
	if (ctx.shd.ctx)
		shd_close(ctx.shd.ctx, ctx.shd.rev);
	if (ctx.timer)
		pomp_timer_destroy(ctx.timer);
	if (ctx.loop)
		pomp_loop_destroy(ctx.loop);

	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}