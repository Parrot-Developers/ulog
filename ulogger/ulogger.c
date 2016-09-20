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
 * Shell command interface to ulog, similar to syslog logger.
 *
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define ULOG_TAG ulogger
#include <ulog.h>

ULOG_DECLARE_TAG(ulogger);

static void usage(void)
{
	fprintf(stderr,
		"Usage: ulogger [-h] [-p PRIO] [-s] [-t TAG] [MESSAGE]\n"
		"   -h       Show this help text\n"
		"   -p PRIO  Specify one-letter prio (C,E,W,N,I,D) or number\n"
		"   -s       Output message to stderr as well\n"
		"   -t TAG   Specify message tag\n");
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

int main(int argc, char *argv[])
{
	char buf[256];
	const char *tag = "";
	const char * const levels = "01CEWNID";
	int c, i, copy_stderr = 0, level = ULOG_INFO;

	while ((c = getopt(argc, argv, "hp:st:")) != -1) {
		switch (c) {
		case 'p':
			level = parse_level(optarg[0]);
			break;
		case 's':
			copy_stderr = 1;
			break;
		case 't':
			tag = optarg;
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	/* change tag name, safe only because there is no concurrent access */
	__ULOG_REF(ulogger).name = tag;
	__ULOG_REF(ulogger).namesize = strlen(tag)+1;

	if (optind < argc) {
		for (i = optind; i < argc; i++) {
			ULOG_STR(level, argv[i]);
			if (copy_stderr) {
				fprintf(stderr, "%c %s: %s\n", levels[level],
					tag, argv[i]);
			}
		}
	} else {
		/* if no message provided, read from stdin */
		while (fgets(buf, sizeof(buf), stdin)) {
			ULOG_STR(level, buf);
			if (copy_stderr) {
				fprintf(stderr, "%c %s: %s", levels[level],
					tag, buf);
				if (!buf[0] || (buf[strlen(buf)-1] != '\n'))
					fprintf(stderr, "\n");
			}
		}
	}

	return 0;
}
