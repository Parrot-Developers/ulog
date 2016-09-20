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
 * ulogcat, a reader for ulogger/logger/klog messages.
 *
 * A few bits are derived from Android logcat.
 *
 */

#include "ulogcat.h"

static void show_help(const char *cmd)
{
	fprintf(stderr, "Usage: %s [options]\n", cmd);

	fprintf(stderr, "options include:\n"
		"  -v <format>     Sets the log print format, where <format> is one of:\n\n"
		"                  short aligned process long csv binary ckcm\n\n"
		"  -c              Clear (flush) the entire log and exit.\n"
		"  -d              Dump the log and then exit (don't block)\n"
		"  -g              Print the size of the log's ring buffer and exit\n"
		"  -p <port>       Accept a client connection on TCP port <port> and redirect output to it.\n"
		"                  Unless option -f is specified, message logging does not start until a first\n"
		"                  initial connection occurs; this allows client to retrieve existing messages.\n"
		"  -k              Include kernel ring buffer messages in output.\n"
		"  -a              Include Android messages in output.\n"
		"  -u              Include ulog messages in output (this is the default if\n"
		"                  none of options -k and -a are specified).\n"
		"  -l              Prefix each message with letter 'U', 'A' or 'K' to indicate\n"
		"                  its origin (Ulog, Android or Kernel). This is useful to split\n"
		"                  an interleaved output.\n"
		"  -b <buffer>     Request alternate ulog buffer, 'main', 'balboa', etc.\n"
		"                  Multiple -b parameters are allowed and the results are\n"
		"                  interleaved. The default is to show all buffers.\n"
		"  -A <buffer>     Request alternate Android buffer, 'main', 'system', etc.\n"
		"                  Multiple -A parameters are allowed and the results are\n"
		"                  interleaved. The default is to show buffers 'main' and 'system'.\n"
		"  -f <path>       Persist and rotate log messages to <path>, <path>.1, <path>.2, etc.\n"
		"                  <path> should exist and contain a directory part. If <path> is relative, then\n"
		"                  mounted partitions are scanned and the first partition whose root directory\n"
		"                  contains the directory part of <path> is used.\n"
		"  -r <kb>         Rotate log every <kb> kB (defaults to 4096 kB). Requires -f.\n"
		"  -n <count>      Sets max number of rotated log files to <count>, default 4. Requires -f.\n"
		"  -w              Block and do not output messages when persistence is disabled or when output\n"
		"                  files cannot be accessed. Requires -f.\n"
		"  -R              Restore persistent setting (enabled or disabled) at startup.\n"
		"  -P <addr>       Enable remote control of persistent logging by creating a libpomp server at\n"
		"                  local address @/com/parrot/ulogcat/<addr>. Requires -f.\n"
		"  -B              Output the log in binary (deprecated, use -v instead).\n"
		"  -C              Use ANSI color sequences to show priority levels; you can customize colors\n"
		"                  used for each level with environment variable ULOGCAT_COLORS, which contains\n"
		"                  (possibly empty) sequences for each of the 8 levels, separated by character\n"
		"                  '|'. Default value: ULOGCAT_COLORS='||4;1;31|1;31|1;33|35||1;30'.\n"
		"  -F <filename>   Get options from <filename>. Remaining options are ignored.\n"
		"\n");
}

/* Get options from a file */
static void reset_options(const char *filename, int *argc, char ***argv)
{
	FILE *fp;
	int extra_argc;
	size_t size = 0;
	char *arg, *s, *line;
	static char buf[4096];
	static char *extra_argv[64];
	const int max_args = (int)(sizeof(extra_argv)/sizeof(extra_argv[0]));

	fp = fopen(filename, "r");
	if (fp) {
		size = fread(buf, 1, sizeof(buf), fp);
		fclose(fp);
	}

	if (size == 0)
		return;

	buf[size-1] = '\0';
	extra_argc = 1;
	s = buf;

	do {
		/* get a NUL-terminated line */
		line = s;
		s = strchr(line, '\n');
		if (s)
			*s++ = '\0';

		arg = strtok(line, " \t");
		/* skip comments */
		if (arg && (arg[0] == '#'))
			continue;
		/* parse line (no quotation supported) */
		while (arg) {
			if (extra_argc+1 < max_args)
				extra_argv[extra_argc++] = arg;
			arg = strtok(NULL, " \t");
		}

	} while (s);

	extra_argv[extra_argc] = NULL;
	if (extra_argc > 1) {
		*argc = extra_argc;
		*argv = extra_argv;
		/* reset getopt() index */
		optind = 1;
	}
}

static int parse_log_format(const char *formatString)
{
	int ret = -1;

	if (formatString) {
		if (strcmp(formatString, "short") == 0)
			ret = ULOGCAT_FORMAT_SHORT;
		else if (strcmp(formatString, "aligned") == 0)
			ret = ULOGCAT_FORMAT_ALIGNED;
		else if (strcmp(formatString, "process") == 0)
			ret = ULOGCAT_FORMAT_PROCESS;
		else if (strcmp(formatString, "long") == 0)
			ret = ULOGCAT_FORMAT_LONG;
		else if (strcmp(formatString, "csv") == 0)
			ret = ULOGCAT_FORMAT_CSV;
		else if (strcmp(formatString, "binary") == 0)
			ret = ULOGCAT_FORMAT_BINARY;
		else if (strcmp(formatString, "ckcm") == 0)
			ret = ULOGCAT_FORMAT_CKCM;
	}

	return ret;
}

void get_options(int argc, char **argv, struct options *op)
{
	int ret;

	memset(op, 0, sizeof(*op));

	op->opts.opt_format = ULOGCAT_FORMAT_ALIGNED;
	op->opts.opt_output_fd = -1;

	if ((argc == 2) && (0 == strcmp(argv[1], "--help"))) {
		show_help(argv[0]);
		exit(0);
	}

	for (;;) {
		ret = getopt(argc, argv, "A:aBb:Ccdf:F:gkln:p:P:r:Rtuv:w");
		if (ret < 0)
			break;

		switch (ret) {
		case 'c':
			op->opts.opt_flags |= ULOGCAT_FLAG_CLEAR;
			break;
		case 'd':
			op->opts.opt_flags |= ULOGCAT_FLAG_DUMP;
			break;
		case 'C':
			op->opts.opt_flags |= ULOGCAT_FLAG_COLOR;
			break;
		case 'g':
			op->opts.opt_flags |= ULOGCAT_FLAG_GET_SIZE;
			break;
		case 'k':
			op->opts.opt_flags |= ULOGCAT_FLAG_KLOG;
			break;
		case 'a':
			op->opts.opt_flags |= ULOGCAT_FLAG_ALOG;
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
		case 'A':
			op->alog_devices = realloc(op->alog_devices,
					       (op->alog_ndevices+1)*
					       sizeof(*op->alog_devices));
			if (op->alog_devices)
				op->alog_devices[op->alog_ndevices++] = optarg;
			op->opts.opt_flags |= ULOGCAT_FLAG_ALOG;
			break;
		case 'B':
			op->opts.opt_format = ULOGCAT_FORMAT_BINARY;
			break;
		case 'f':
			op->persist_filename = optarg;
			break;
		case 'F':
			reset_options(optarg, &argc, &argv);
			break;
		case 'p':
			if (!isdigit(optarg[0])) {
				fprintf(stderr, "Invalid parameter to -p\n");
				show_help(argv[0]);
				exit(-1);
			}
			op->port = atoi(optarg);
			break;
		case 'r':
			if (!isdigit(optarg[0])) {
				fprintf(stderr, "Invalid parameter to -r\n");
				show_help(argv[0]);
				exit(-1);
			}
			op->persist_size = atoi(optarg);
			break;
		case 'n':
			if (!isdigit(optarg[0])) {
				fprintf(stderr, "Invalid parameter to -n\n");
				show_help(argv[0]);
				exit(-1);
			}
			op->persist_logs = atoi(optarg);
			break;
		case 'w':
			op->persist_blocking_mode = 1;
			break;
		case 'R':
			op->persist_restore = 1;
			break;
		case 'P':
			op->persist_pomp_addr = optarg;
			break;
		case 't':
			/* undocumented option, used for tests */
			op->persist_test_client = 1;
			break;
		case 'v':
			ret = parse_log_format(optarg);
			if (ret < 0) {
				fprintf(stderr, "Invalid parameter to -v\n");
				show_help(argv[0]);
				exit(-1);
			}
			op->opts.opt_format = ret;
			break;
		default:
			fprintf(stderr, "Unrecognized option\n");
			show_help(argv[0]);
			exit(-1);
			break;
		}
	}

	if (op->persist_filename) {
		/* provide default values */
		if (op->persist_size == 0)
			op->persist_size = 4096;
		if (op->persist_logs == 0)
			op->persist_logs = 4;
	} else if (op->persist_restore) {
		fprintf(stderr, "Option -R requires -f\n");
		show_help(argv[0]);
		exit(-1);
	} else if (op->persist_pomp_addr && !op->persist_test_client) {
		fprintf(stderr, "Option -P requires -f\n");
		show_help(argv[0]);
		exit(-1);
	}

	if (!(op->opts.opt_flags & (ULOGCAT_FLAG_ULOG|
				    ULOGCAT_FLAG_ALOG|
				    ULOGCAT_FLAG_KLOG)))
		/* default output is ulog buffers */
		op->opts.opt_flags |= ULOGCAT_FLAG_ULOG;
}
