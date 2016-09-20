/**
 * Copyright (C) 2016 Parrot S.A.
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
 * @file ulogctl-smp-srv.c
 *
 * @brief Sample implementing a ulogctl_srv to test libulogctl
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>

#include <libpomp.h>
#define ULOG_TAG ulogctl_srv_app
#include <ulog.h>
ULOG_DECLARE_TAG(ulogctl_srv_app);
#include <ulogctl.h>

/** Log error with errno */
#define LOG_ERRNO(_fct, _err) \
	ULOGE("%s:%d: %s err=%d(%s)", __func__, __LINE__, \
			_fct, _err, strerror(_err))

#define LOG_PERIOD 1000

/** */
struct app {
	struct pomp_loop     *loop;
	struct pomp_timer    *timer;
	int                  stopped;
	int                  log_id;
	struct ulogctl_srv   *ulogctl;
};

static struct app s_app = {
	.loop = NULL,
	.timer = NULL,
	.stopped = 0,
	.log_id = 0,
	.ulogctl = NULL,
};

/**
 */
static void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [<options>]\n", progname);
	fprintf(stderr, "Ulog controller server\n"
			"\n"
			"  <options>: see below\n"
			"\n"
			"  -h --help : print this help message and exit\n"
			"  -i --inet <port> : Set inet port to use\n"
			"  -u --unix <sock> : Set unix socket name to use\n"
			"  -p --process : Use process name "
			"as abstract unix socket\n"
			"\n"
			"    <port> : Inet port to use\n"
			"    <sock> : Unix socket name to use\n"
			"\n");
}

/**
 */
static void timer_cb(struct pomp_timer *timer, void *userdata)
{
	struct app *app = userdata;

	ULOGD("log debug %d", app->log_id);
	ULOGI("log info %d", app->log_id);
	ULOGN("log normal %d", app->log_id);
	ULOGW("log warning %d", app->log_id);
	ULOGE("log error %d", app->log_id);
	ULOGC("log critical %d", app->log_id);

	app->log_id += 1;
}

/**
 */
static void sig_handler(int signum)
{
	ULOGI("signal %d(%s) received", signum, strsignal(signum));
	s_app.stopped = 1;
	if (s_app.loop != NULL)
		pomp_loop_wakeup(s_app.loop);
}

/**
 */
int main(int argc, char *argv[])
{
	int arg = 0;
	int argidx = 0;
	int port = -1;
	char *sock = NULL;
	int process = 0;
	int res = 0;

	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"inet", required_argument, 0, 'i'},
		{"unix", required_argument, 0, 'u'},
		{"process", no_argument, 0, 'p'},
		{0, 0, 0, 0}
	};

	/* Setup signal handlers */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);
	signal(SIGPIPE, SIG_IGN);

	/* Parse options */
	for (;;) {
		arg = getopt_long (argc, argv, "hi:u:p",
				long_options, &argidx);

		/* Detect the end of the options. */
		if (arg < 0)
			break;

		switch (arg) {
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		case 'i':
			port = atoi(optarg);
			break;
		case 'u':
			sock = optarg;
			break;
		case 'p':
			process = 1;
			break;
		default:
			fprintf(stderr, "Unrecognized option\n");
			usage(argv[0]);
			exit(-1);
			break;
		}
	}

	/* Create loop */
	s_app.loop = pomp_loop_new();
	s_app.timer = pomp_timer_new(s_app.loop, &timer_cb, &s_app);

	if (sock != NULL) {
		fprintf(stderr, "ulogctl_srv_new unix socket: %s\n", sock);
		res = ulogctl_srv_new_unix(sock, s_app.loop,
				&s_app.ulogctl);
		if (res < 0) {
			LOG_ERRNO("ulogctl_srv_new_unix", -res);
			goto out;
		}
	} else if (port != -1) {
		fprintf(stderr, "ulogctl_srv_new inet port: %d\n", port);
		res = ulogctl_srv_new_inet(port, s_app.loop, &s_app.ulogctl);
		if (res < 0) {
			LOG_ERRNO("ulogctl_srv_new_inet", -res);
			goto out;
		}
	} else if (process) {
		fprintf(stderr, "ulogctl_srv_new unix process\n");
		res = ulogctl_srv_new_unix_proc(s_app.loop, &s_app.ulogctl);
		if (res < 0) {
			LOG_ERRNO("ulogctl_srv_new_unix_proc", -res);
			goto out;
		}
	} else {
		fprintf(stderr, "use %s -h\n", argv[0]);
		goto out;
	}

	res = ulogctl_srv_start(s_app.ulogctl);
	if (res < 0) {
		LOG_ERRNO("ulogctl_srv_start", -res);
		goto out;
	}

	/* Setup periodic timer to send 'PCMD' commands */
	res = pomp_timer_set_periodic(s_app.timer, LOG_PERIOD, LOG_PERIOD);
	if (res < 0) {
		LOG_ERRNO("pomp_timer_set_periodic", -res);
		goto out;
	}

	/* Run loop */
	while (!s_app.stopped)
		pomp_loop_wait_and_process(s_app.loop, -1);

	/* Cleanup */
out:
	if (s_app.timer != NULL) {
		res = pomp_timer_clear(s_app.timer);
		if (res < 0)
			LOG_ERRNO("pomp_timer_clear", -res);
	}

	if (s_app.ulogctl != NULL) {
		res = ulogctl_srv_stop(s_app.ulogctl);
		if (res < 0)
			LOG_ERRNO("ulogctl_srv_stop", -res);

		res = ulogctl_srv_destroy(s_app.ulogctl);
		if (res < 0)
			LOG_ERRNO("ulogctl_srv_destroy", -res);
	}

	if (s_app.timer != NULL) {
		res = pomp_timer_destroy(s_app.timer);
		if (res < 0)
			LOG_ERRNO("pomp_timer_destroy", -res);
	}

	if (s_app.loop != NULL) {
		res = pomp_loop_destroy(s_app.loop);
		if (res < 0)
			LOG_ERRNO("pomp_loop_destroy", -res);
	}

	return 0;
}
