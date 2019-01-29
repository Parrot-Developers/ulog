/**
 * Copyright (C) 2013 Parrot S.A.
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
 * ulogcat, a reader for ulogger/klog messages.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <libulogcat.h>

#define INFO(...)        fprintf(stderr, "ulogcat: " __VA_ARGS__)

struct options {
	struct ulogcat_opts_v3  opts;
	int                     opt_clear;
	char                  **ulog_devices;
	int                     ulog_ndevices;
};

static void show_usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s [options]\n", cmd);

	fprintf(stderr, "options include:\n"
		"  -v <format>     Sets the log print format, where <format> is"
		" one of:\n\n"
		"                  short aligned process long csv\n\n"
		"  -c              Clear (flush) the entire log and exit.\n"
		"  -d              Dump the log and then exit (don't block)\n"
		"  -k              Include kernel ring buffer messages in "
		"output.\n"
		"  -u              Include ulog messages in output (this is the"
		" default if\n"
		"                  none of options -k and -u are specified).\n"
		"  -l              Prefix each message with letter 'U' or 'K' "
		"to indicate\n"
		"                  its origin (Ulog, Kernel). This is useful to"
		" split\n"
		"                  an interleaved output.\n"
		"  -b <buffer>     Request alternate ulog buffer, 'main', "
		"'balboa', etc.\n"
		"                  Multiple -b parameters are allowed and the "
		"results are\n"
		"                  interleaved. The default is to show all "
		"buffers.\n"
		"  -C              Use ANSI color sequences to show priority "
		"levels; you can customize colors\n"
		"                  used for each level with environment "
		"variable ULOGCAT_COLORS, which contains\n"
		"                  (possibly empty) sequences for each of the "
		"8 levels, separated by character\n"
		"                  '|'. Default value: "
		"ULOGCAT_COLORS='||4;1;31|1;31|1;33|35||1;30'.\n"
		"  -t <n>          Skip entries and show only <n> tail lines\n"
		"  -h              Show this help\n"
		"\n");
}

static int parse_log_format(const char *str)
{
	int ret = -1;

	if (str) {
		if (strcmp(str, "short") == 0)
			ret = ULOGCAT_FORMAT_SHORT;
		else if (strcmp(str, "aligned") == 0)
			ret = ULOGCAT_FORMAT_ALIGNED;
		else if (strcmp(str, "process") == 0)
			ret = ULOGCAT_FORMAT_PROCESS;
		else if (strcmp(str, "long") == 0)
			ret = ULOGCAT_FORMAT_LONG;
		else if (strcmp(str, "csv") == 0)
			ret = ULOGCAT_FORMAT_CSV;
	}

	return ret;
}

static void get_options(int argc, char **argv, struct options *op)
{
	int ret;

	memset(op, 0, sizeof(*op));
	op->opts.opt_format = ULOGCAT_FORMAT_ALIGNED;

	for (;;) {
		ret = getopt(argc, argv, "b:Ccdhklt:uv:");
		if (ret < 0)
			break;

		switch (ret) {
		case 'c':
			op->opt_clear = 1;
			break;
		case 'd':
			op->opts.opt_flags |= ULOGCAT_FLAG_DUMP;
			break;
		case 'C':
			op->opts.opt_flags |= ULOGCAT_FLAG_COLOR;
			break;
		case 'k':
			op->opts.opt_flags |= ULOGCAT_FLAG_KLOG;
			break;
		case 'u':
			op->opts.opt_flags |= ULOGCAT_FLAG_ULOG;
			break;
		case 'l':
			op->opts.opt_flags |= ULOGCAT_FLAG_SHOW_LABEL;
			break;
		case 'b':
			op->ulog_devices = realloc(op->ulog_devices,
						   (op->ulog_ndevices+1)*
						   sizeof(*op->ulog_devices));
			if (op->ulog_devices)
				op->ulog_devices[op->ulog_ndevices++] = optarg;
			op->opts.opt_flags |= ULOGCAT_FLAG_ULOG;
			break;
		case 't':
			op->opts.opt_tail = atoi(optarg);
			break;
		case 'h':
			show_usage(argv[0]);
			exit(0);
			break;
		case 'v':
			ret = parse_log_format(optarg);
			if (ret < 0) {
				fprintf(stderr, "Invalid parameter to -v\n");
				show_usage(argv[0]);
				exit(-1);
			}
			op->opts.opt_format = ret;
			break;
		default:
			fprintf(stderr, "Unrecognized option\n");
			show_usage(argv[0]);
			exit(-1);
			break;
		}
	}

	if (!(op->opts.opt_flags & (ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_KLOG)))
		/* default output is ulog buffers */
		op->opts.opt_flags |= ULOGCAT_FLAG_ULOG;
}

static int process_logs(struct ulogcat3_context *ctx, int max_entries,
			unsigned int idle_ms)
{
	int ret;

	while (1) {

		/* this will block until some entries are available */
		ret = ulogcat3_process_logs(ctx, max_entries);
		if (ret <= 0)
			break;

		/* optionally throttle CPU usage */
		if ((max_entries > 0) && idle_ms && (ret == 1))
			usleep(idle_ms*1000);
	}

	return ret;
}

int main(int argc, char **argv)
{
	int ret = -1;
	struct options op;
	struct ulogcat3_context *ctx;

	get_options(argc, argv, &op);
	op.opts.opt_output_fp = stdout;

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	ctx = ulogcat3_open(&op.opts, (const char **)op.ulog_devices,
			    op.ulog_ndevices);
	if (ctx == NULL) {
		INFO("cannot open ulogcat context\n");
		goto finish;
	}

	/* get specific actions (clear) out of the way */
	if (op.opt_clear) {
		ret = ulogcat3_clear(ctx);
		goto finish;
	}

	ret = process_logs(ctx, 0, 0);

finish:
	ulogcat3_close(ctx);
	free(op.ulog_devices);

	return ret;
}
