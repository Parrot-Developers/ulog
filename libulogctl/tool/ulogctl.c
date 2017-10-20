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

 * @file ulogctl.c
 *
 * @brief Tool of ulogctl client.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <stddef.h>
#include <sys/un.h>

#include <libpomp.h>
#include <ulog.h>

#include <unistd.h>
#include <sys/socket.h>

#include <ulogctl.h>

#define COLOR_RESET  "\x1B[0m"
#define COLOR_RED    "\x1B[00;91m"
#define COLOR_GREEN  "\x1B[00;92m"
#define COLOR_YELLOW "\x1B[00;93m"
#define COLOR_BLUE   "\x1B[00;94m"
#define COLOR_PURPLE "\x1B[00;95m"

/** Log error with errno */
#define LOG_ERRNO(_fct, _err) \
	fprintf(stderr, "[E] %s:%d: %s err=%d(%s)\n", __func__, __LINE__, \
			_fct, _err, strerror(_err))

/** Log error with fd and errno */
#define LOG_FD_ERRNO(_fct, _fd, _err) \
	fprintf(stderr, "[E] %s:%d: %s(fd=%d) err=%d(%s)\n", __func__, \
			__LINE__, _fct, _fd, _err, strerror(_err))

/** Log error if condition failed and return from function */
#define RETURN_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			fprintf(stderr, "[E] %s:%d: err=%d(%s)\n", __func__, \
			__LINE__, (_err), strerror(-(_err))); \
			return; \
		} \
	} while (0)

/** Log error if condition failed and return error from function */
#define RETURN_ERR_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			fprintf(stderr, "[E] %s:%d: err=%d(%s)\n", __func__, \
			__LINE__, (_err), strerror(-(_err))); \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_err); \
		} \
	} while (0)

/** Log error if condition failed and return value from function */
#define RETURN_VAL_IF_FAILED(_cond, _err, _val) \
	do { \
		if (!(_cond)) { \
			fprintf(stderr, "[E] %s:%d: err=%d(%s)\n", __func__, \
			__LINE__, (_err), strerror(-(_err))); \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_val); \
		} \
	} while (0)

/** */
struct app {
	struct pomp_loop        *loop;
	int                     stopped;
	struct ulogctl_cli      *ulogctl_cli;
	struct sockaddr_storage addr;
	uint32_t                addrlen;
	int                     use_color;
};

static struct app s_app = {
	.loop = NULL,
	.stopped = 0,
	.ulogctl_cli = NULL,
	.addrlen = 0,
	.use_color = 0,
};

/**
 */
static void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [<options>] <addr>\n", progname);
	fprintf(stderr, "       %s [<options>] -p <proc>\n", progname);
	fprintf(stderr, "Ulog controller client\n"
			"\n"
			"  <options>: see below\n"
			"  <addr>  : address\n"
			"\n"
			"<addr> format:\n"
			"  inet:<addr>:<port>\n"
			"  inet6:<addr>:<port>\n"
			"  unix:<path>\n"
			"  unix:@<name>\n"
			"\n"
			"  -h --help : print this help message and exit\n"
			"  -l --list : List of tags known\n"
			"  -t --tag <tag> <level> : Set log level for a tag\n"
			"  -a --all <level> : Set all log levels\n"
			"  -C --color : Enable colored tags\n"
			"  -p --process <proc> : Set process name\n"
			"\n"
			"    <tag>   : tag name\n"
			"    <level> : log level to set\n"
			"    <proc>  : process name to use instead of address\n"
			"\n"
			"<level> format: C; E; W; N; I; D\n"
			"\n");
}

/**
 */
static int char2level(const char lev_char)
{
	if (lev_char == 'c' || lev_char == 'C')
		return ULOG_CRIT;
	else if (lev_char == 'e' || lev_char == 'E')
		return ULOG_ERR;
	else if (lev_char == 'w' || lev_char == 'W')
		return ULOG_WARN;
	else if (lev_char == 'n' || lev_char == 'N')
		return ULOG_NOTICE;
	else if (lev_char == 'i' || lev_char == 'I')
		return ULOG_INFO;
	else if (lev_char == 'd' || lev_char == 'D')
		return ULOG_DEBUG;

	return -1;
}

/**
 */
static char level2char(const int level)
{
	switch (level) {
	case ULOG_CRIT:
		return 'C';
		break;
	case ULOG_ERR:
		return 'E';
		break;
	case ULOG_WARN:
		return 'W';
		break;
	case ULOG_NOTICE:
		return 'N';
		break;
	case ULOG_INFO:
		return 'I';
		break;
	case ULOG_DEBUG:
		return 'D';
		break;
	default:
		return '?';
		break;
	}

	return '?';
}

static void request_status_cb(enum ulogctl_req_status status,
		void *userdata)
{
	struct app *app = userdata;

	RETURN_IF_FAILED(app != NULL, -EINVAL);

	if (status == REQUEST_ERROR)
		fprintf(stderr, "Error occurred.\n");

	app->stopped = 1;
}

static const char *level_to_color(int level)
{
	switch (level) {
	case ULOG_CRIT:   return COLOR_RED;
	case ULOG_ERR:    return COLOR_RED;
	case ULOG_WARN:   return COLOR_YELLOW;
	case ULOG_NOTICE: return COLOR_GREEN;
	case ULOG_INFO:   return COLOR_BLUE;
	case ULOG_DEBUG:  return COLOR_PURPLE;
	default:          return COLOR_RESET;
	}
}

static void tag_info_cb(const char *tag, int level, void *userdata)
{
	char level_char;
	const char *color;
	const char *color_reset;
	struct app *app = userdata;

	RETURN_IF_FAILED(app != NULL, -EINVAL);

	color = app->use_color ? level_to_color(level) : "";
	color_reset = app->use_color ? COLOR_RESET : "";
	level_char = level2char(level);
	fprintf(stderr, "%s[%c] %s%s\n", color, level_char, tag, color_reset);
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

static int parse_addr(struct app *app, const char *arg_addr)
{
	struct sockaddr_un *addr_un = NULL;

	memset(&app->addr, 0, sizeof(app->addr));
	app->addrlen = sizeof(app->addr);

	if (strncmp(arg_addr, "unix:", 5) == 0) {
		/* Unix address */
		if (app->addrlen < sizeof(struct sockaddr_un))
			return -EINVAL;
		addr_un = (struct sockaddr_un *)&app->addr;
		memset(addr_un, 0, sizeof(*addr_un));
		addr_un->sun_family = AF_UNIX;
		strncpy(addr_un->sun_path, arg_addr + 5,
				sizeof(addr_un->sun_path));
		if (arg_addr[5] == '@')
			addr_un->sun_path[0] = '\0';
		app->addrlen = offsetof(struct sockaddr_un, sun_path) +
				strlen(arg_addr + 5);

		return 0;
	} else {
		return pomp_addr_parse(arg_addr, (struct sockaddr *)&app->addr,
				&app->addrlen);
	}
}

/**
 */
int main(int argc, char *argv[])
{
	int arg = 0;
	int argidx = 0;
	int res = 0;
	int set_tag_level = 0;
	int set_all_level = 0;
	int get_list = 0;
	char *tag = NULL;
	char level_arg = '\0';
	int level = -1;
	char *proc_name = NULL;

	const char *arg_addr = NULL;
	struct ulogctl_cli_cbs ulogctl_cbs;

	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"list", no_argument, 0, 'l'},
		{"color", no_argument, 0, 'C'},
		{"tag", required_argument, 0, 't'},
		{"all", required_argument, 0, 'a'},
		{"process", required_argument, 0, 'p'},
		{0, 0, 0, 0}
	};

	/* Setup signal handlers */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);
	signal(SIGPIPE, SIG_IGN);

	/* Parse options */
	for (;;) {
		arg = getopt_long (argc, argv, "hlCt:a:p:",
				long_options, &argidx);

		/* Detect the end of the options. */
		if (arg < 0)
			break;

		switch (arg) {
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		case 'l':
			get_list = 1;
			break;
		case 'C':
			s_app.use_color = 1;
			break;
		case 't':
			tag = optarg;

			/* get log level argument */
			if ((optind < argc) && (argv[optind][0] != '-')) {
				level_arg = argv[optind][0];
				optind++;
				set_tag_level = 1;
			} else {
				fprintf(stderr, "Missing log level\n");
				exit(-1);
			}

			/* Check level argument */
			level = char2level(level_arg);
			if (level < 0) {
				fprintf(stderr, "Unrecognized level (%c).\n",
						level_arg);
				exit(-1);
			}

			break;
		case 'a':
			level_arg = optarg[0];

			/* Check level argument */
			level = char2level(level_arg);
			if (level < 0) {
				fprintf(stderr, "Unrecognized level (%c).\n",
						level_arg);
				exit(-1);
			}

			set_all_level = 1;

			break;
		case 'p':
			proc_name = optarg;
			break;
		default:
			fprintf(stderr, "Unrecognized option\n");
			usage(argv[0]);
			exit(-1);
			break;
		}
	}

	/* Get address */
	if (optind < argc) {
		arg_addr = argv[optind++];

		/* Parse address */
		if (parse_addr(&s_app, arg_addr) < 0) {
			fprintf(stderr, "Failed to parse address : %s\n",
					arg_addr);
			goto out;
		}
	} else if (proc_name == NULL) {
		fprintf(stderr, "Missing address or process name\n");
		usage(argv[0]);
		exit(-1);
	}

	/* Create loop */
	s_app.loop = pomp_loop_new();

	/* Create ulog controller client */
	memset(&ulogctl_cbs, 0, sizeof(ulogctl_cbs));
	ulogctl_cbs.userdata = &s_app;
	ulogctl_cbs.request_status = &request_status_cb;
	ulogctl_cbs.tag_info = &tag_info_cb;

	if (proc_name != NULL) {
		res = ulogctl_cli_new_proc(proc_name, s_app.loop, &ulogctl_cbs,
			&s_app.ulogctl_cli);
		if (res < 0) {
			LOG_ERRNO("ulogctl_cli_new_proc", -res);
			goto out;
		}
	} else {
		res = ulogctl_cli_new((struct sockaddr *)&s_app.addr,
				s_app.addrlen, s_app.loop, &ulogctl_cbs,
				&s_app.ulogctl_cli);
		if (res < 0) {
			LOG_ERRNO("ulogctl_cli_new", -res);
			goto out;
		}
	}

	res = ulogctl_cli_start(s_app.ulogctl_cli);
	if (res < 0) {
		LOG_ERRNO("ulogctl_cli_start", -res);
		goto out;
	}

	if (set_tag_level) {
		res = ulogctl_cli_set_tag_level(s_app.ulogctl_cli,
					tag, level);
		if (res < 0)
			LOG_ERRNO("ulogctl_cli_set_tag_level", -res);
	} else if (set_all_level) {
		res = ulogctl_cli_set_all_level(s_app.ulogctl_cli, level);
		if (res < 0)
			LOG_ERRNO("ulogctl_cli_set_all_level", -res);
	} else if (get_list) {
		res = ulogctl_cli_list(s_app.ulogctl_cli);
		if (res < 0)
			LOG_ERRNO("ulogctl_cli_list", -res);
	}

	/* Run loop */
	while (!s_app.stopped)
		pomp_loop_wait_and_process(s_app.loop, -1);

out:
	if (s_app.ulogctl_cli != NULL) {
		res = ulogctl_cli_stop(s_app.ulogctl_cli);
		if (res < 0)
			LOG_ERRNO("ulogctl_cli_stop", -res);

		res = ulogctl_cli_destroy(s_app.ulogctl_cli);
		if (res < 0)
			LOG_ERRNO("ulogctl_cli_destroy", -res);
	}

	if (s_app.loop != NULL) {
		res = pomp_loop_destroy(s_app.loop);
		if (res < 0)
			LOG_ERRNO("pomp_loop_destroy", -res);
	}

	return 0;
}
