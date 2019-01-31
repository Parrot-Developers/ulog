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
 * Redirect syslog calls to libulog
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ulogger.h>

#define WRAPPER "/usr/lib/libulog_syslogwrap.so"

int main(int argc, char *argv[])
{
	int fd, ret;
	static char buf[4096];
	char devbuf[32];
	const char *dev, *libs, *prop, *value = WRAPPER;

	if (argc < 2) {
		fprintf(stderr, "Usage: ulogwrapper <filename> <args>\n");
		return EXIT_FAILURE;
	}

	dev = "/dev/" ULOGGER_LOG_MAIN;
	prop = getenv("ULOG_DEVICE");
	if (prop) {
		snprintf(devbuf, sizeof(devbuf), "/dev/ulog_%s", prop);
		dev = devbuf;
	}

	fd = open(dev, O_WRONLY);
	if (fd >= 0) {
		close(fd);
		libs = getenv("LD_PRELOAD");
		if (libs) {
			if (strstr(libs, WRAPPER))
				/* already wrapped */
				goto finish;
			/* prepend our wrapper library */
			snprintf(buf, sizeof(buf), WRAPPER" %s", libs);
			value = buf;
		}
		/* coverity[tainted_string] */
		setenv("LD_PRELOAD", value, 1);
		/* make sure we disable syslog fallback in libulog */
		setenv("ULOG_NOSYSLOG", "yes", 1);
	}
finish:
	/* coverity[tainted_string] */
	ret = execve(argv[1], argv+1, environ);
	if (ret < 0)
		fprintf(stderr, "execve('%s'): %m\n", argv[1]);
	return ret;
}
