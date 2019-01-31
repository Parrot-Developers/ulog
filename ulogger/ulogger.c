/**
 * Copyright (C) 2013 Parrot S.A.
 * Copyright (C) 2018 Parrot Drones
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
 * Shell command interface to ulog, similar to syslog logger.
 *
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <ulograw.h>

#define ULOG_TAG ulogger
#include <ulog.h>
ULOG_DECLARE_TAG(ulogger);
#define ULOG_DEFAULT_DEVICE "main"

/* codecheck_ignore[AVOID_EXTERNS] */
extern const char *program_invocation_short_name;

static void usage(void)
{
	fprintf(stderr,
		"Usage: ulogger [ -h/--help ] [ -i/--pid PID ] [ -m/--time ]\n"
		"               [ -n/--name NAME ] [ -p/--prio PRIO ] [ -s/--stderr ]\n"
		"               [ -t/--tag TAG ] [ TIME ] [ MESSAGE ]\n"
		"   -h, --help       Show this help text\n"
		"   -i, --pid  PID   Override log entry process pid\n"
		"   -m, --time       Override log entry timestamp with TIME (number of\n"
		"                    seconds optionally followed by a space and the\n"
		"                    number of nanoseconds)\n"
		"   -n, --name NAME  Override log entry process name\n"
		"   -p, --prio PRIO  Specify one-letter prio (C,E,W,N,I,D) or number\n"
		"   -s, --stderr     Output message to stderr as well\n"
		"   -t, --tag  TAG   Specify message tag\n");
}

/* parse a log level description (letter or digit) */
static int parse_level(int c)
{
	int level;

	if (isdigit(c)) {
		level = c-'0';
		if (level > ULOG_DEBUG)
			level = ULOG_DEBUG;
	} else {
		switch (c) {
		case 'C':
			level = ULOG_CRIT;
			break;
		case 'E':
			level = ULOG_ERR;
			break;
		case 'W':
			level = ULOG_WARN;
			break;
		case 'N':
			level = ULOG_NOTICE;
			break;
		case 'I':
			level = ULOG_INFO;
			break;
		case 'D':
			level = ULOG_DEBUG;
			break;
		default:
			level = ULOG_INFO;
			break;
		}
	}

	return level;
}

static int parse_int32(const char *str, int32_t *value, const char **pendptr)
{
	char *endptr;
	int ret;
	if (!str || *str == '\0' || !value)
		return -EINVAL;
	ret = strtol(str, &endptr, 10);
	if (pendptr)
		*pendptr = endptr;
	if (*endptr == '\0') {
		*value = ret;
		return 0;
	} else {
		return -EINTR;
	}
}

static int parse_time(
	const char *str, int32_t *seconds, int32_t *nanoseconds,
	const char **pendptr)
{
	const char *endptr;
	int s, n;
	int ret;

	if (!str || *str == '\0' || *str == ' ' || !seconds || !nanoseconds)
		return -EINVAL;

	s = strtol(str, (char **)&endptr, 10);
	if (*endptr == ' ') {
		*seconds = s;
		/* nanoseconds is optional and defaults to 0 */
		*nanoseconds = 0;
		endptr++;
		str = endptr;
		n = strtol(str, (char **)&endptr, 10);
		if (str != endptr)
			*nanoseconds = n;
		ret = 0;
	} else {
		ret = -EINTR;
	}

	if (pendptr)
		*pendptr = endptr;
	return ret;
}

static const char *space_skip(const char *p)
{
	if (!p)
		return p;

	while (*p == ' ' && *p != '\0')
		p++;

	return p;
}

static int ulogger_log(int ulogfd, struct ulog_raw_entry *raw, int copy_stderr)
{
	int ret = 0;
	const char *const prios = "01CEWNID";
	if (ulogfd >= 0)
		ret = ulog_raw_log(ulogfd, raw);
	else
		ULOG_STR(raw->prio, raw->message);
	if (ret < 0) {
		fprintf(stderr, "ulog_raw_log error: %s\n", strerror(-ret));
		exit(-ret);
	}
	if (copy_stderr) {
		fprintf(stderr, "%c %s: %s", prios[raw->prio],
			raw->tag, raw->message);
		if (!raw->message[0] ||
				(raw->message[strlen(raw->message)-1] != '\n'))
			fprintf(stderr, "\n");
	}
	return ret;
}

int main(int argc, char *argv[])
{
	char buf[256];
	char path[128];
	struct timespec ts;
	int c, i, copy_stderr = 0, has_time = 0;
	const char *parse_pos;
	struct ulog_raw_entry raw;
	struct option long_options[] = {
		{"help",    no_argument,       0,           'h'},
		{"pid",     required_argument, 0,           'i'},
		{"time",    no_argument,       &has_time,   1},
		{"name",    required_argument, 0,           'n'},
		{"prio",    required_argument, 0,           'p'},
		{"stderr",  no_argument,       &copy_stderr, 1},
		{"tag",     required_argument, 0,           't'},
		{0, 0, 0, 0}
	};
	const char *ulogdev = getenv("ULOG_DEVICE");
	int ulogfd;

	memset(&raw, 0, sizeof(raw));
	raw.pname = program_invocation_short_name;
	raw.pname_len = strlen(raw.pname) + 1;
	raw.tname = NULL;
	raw.tname_len = 0;
	raw.entry.pid = getpid();
	raw.entry.tid = raw.entry.pid;
	raw.prio = ULOG_INFO;
	raw.tag = "ulogger";
	raw.tag_len = strlen(raw.tag) + 1;

	if (!ulogdev)
		ulogdev = ULOG_DEFAULT_DEVICE;
	snprintf(path, sizeof(path), "/dev/ulog_%s", ulogdev);
	ulogfd = ulog_raw_open(path);
	if (ulogfd < 0) {
		fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
		/* change tag name, safe only because there is no concurrent
		 * access */
		__ULOG_REF(ulogger).name = raw.tag;
		__ULOG_REF(ulogger).namesize = strlen(raw.tag) + 1;
	}

	while ((c = getopt_long(argc, argv, "hi:mn:p:st:", long_options, NULL))
		!= -1) {
		switch (c) {
		case 0:
			/* getopt flag, nothing to do */
			break;
		case 'i':
			if (!parse_int32(optarg, &raw.entry.pid, NULL))
				raw.entry.tid = raw.entry.pid;
			break;
		case 'm':
			has_time = 1;
			break;
		case 'n':
			raw.pname = optarg;
			raw.pname_len = strlen(raw.pname) + 1;
			break;
		case 'p':
			raw.prio = parse_level(optarg[0]);
			break;
		case 's':
			copy_stderr = 1;
			break;
		case 't':
			raw.tag = optarg;
			raw.tag_len = strlen(raw.tag) + 1;
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	if (optind < argc) {
		for (i = optind; i < argc; i++) {
			if (!has_time && !clock_gettime(CLOCK_MONOTONIC, &ts)) {
				raw.entry.sec = ts.tv_sec;
				raw.entry.nsec = ts.tv_nsec;
			} else {
				if (i < argc - 1 && !parse_int32(argv[i],
						&raw.entry.sec, NULL)) {
					i++;
					if (i < argc - 1 && !parse_int32(
							argv[i],
							&raw.entry.nsec,
							NULL)) {
						i++;
					}
				}
			}
			raw.message = argv[i];
			raw.message_len = strlen(raw.message) + 1;
			ulogger_log(ulogfd, &raw, copy_stderr);
		}
	} else {
		/* if no message provided, read from stdin */
		while (fgets(buf, sizeof(buf), stdin)) {
			if (!has_time && !clock_gettime(CLOCK_MONOTONIC, &ts)) {
				raw.entry.sec = ts.tv_sec;
				raw.entry.nsec = ts.tv_nsec;
				raw.message = buf;
				raw.message_len = strlen(raw.message) + 1;
			} else {
				parse_pos = buf;
				parse_time(buf, &raw.entry.sec, &raw.entry.nsec,
						&parse_pos);
				raw.message = space_skip(parse_pos);
				raw.message_len = strlen(raw.message) + 1;
			}
			ulogger_log(ulogfd, &raw, copy_stderr);
		}
	}

	return 0;
}
