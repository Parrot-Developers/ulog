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
 * libulog: a minimalistic logging library derived from Android logger
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#ifndef _WIN32
#  include <sys/uio.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>

#include "ulog.h"
#include "ulogger.h"
#include "ulog_common.h"

/* this cookie is used when ULOG_TAG is undefined */
ULOG_EXPORT struct ulog_cookie __ulog_default_cookie = {
	.name     = "",
	.namesize = 1,
	.level    = -1,
	.userdata = NULL,
	.next     = NULL,
};

static void __writer_init(uint32_t prio, struct ulog_cookie *cookie,
			  const char *buf, int len);

static struct {
	pthread_mutex_t     lock;    /* protect against init race conditions */
	int                 fd;      /* kernel logger file descriptor */
	ulog_write_func_t   writer;  /* output callback */
	ulog_write_func_t   writer2; /* for stderr wrapper */
	/* cookie register hook */
	ulog_cookie_register_func_t cookie_register_hook;
	struct ulog_cookie *cookie_list;
} ctrl = {
	.lock        = PTHREAD_MUTEX_INITIALIZER,
	.fd          = -1,
	.writer      = __writer_init,
	.writer2     = NULL,
	.cookie_register_hook = NULL,
	.cookie_list = NULL,
};

/* null writer (used when both ulogger and syslog are disabled) */
static void __writer_null(uint32_t prio __unused,
			  struct ulog_cookie *cookie __unused,
			  const char *buf __unused,
			  int len __unused)
{
}

/* ulogger writer */
#ifndef _WIN32
static void __writer_kernel(uint32_t prio, struct ulog_cookie *cookie,
			    const char *buf, int len)
{
	ssize_t ret;
	struct iovec vec[3];

	/* priority, color, binary flags, ... */
	vec[0].iov_base = (void *)&prio;
	vec[0].iov_len = 4;
	/* tag, must be null-terminated */
	vec[1].iov_base = (void *)cookie->name;
	vec[1].iov_len = cookie->namesize;
	/* payload: null-terminated string or binary data */
	vec[2].iov_base = (void *)buf;
	vec[2].iov_len = len;

	/* send everything to kernel */
	do {
		ret = writev(ctrl.fd, vec, 3);
	} while ((ret < 0) && (errno == EINTR));
}
#endif

/* copy log messages to stderr */
static void __writer_stderr(uint32_t prio, struct ulog_cookie *cookie,
			    const char *buf, int len, const char *cprio)
{
	ctrl.writer2(prio, cookie, buf, len);

	if (prio & (1U << ULOG_PRIO_BINARY_SHIFT))
		/* skip binary data */
		return;

	fprintf(stderr, "%s %s: %s%s", cprio, cookie->name, buf,
		((len >= 2) && (buf[len-2] == '\n')) ? "" : "\n");
}

/* copy log messages to stderr with color */
static void __writer_stderr_wrapper_color(uint32_t prio,
					  struct ulog_cookie *cookie,
					  const char *buf, int len)
{
	/* log level indicators for logging in color */
	static const char * const priotab[8] = {
		" ",
		" ",
		"\e[7;31mC\e[0m",
		"\e[1;31mE\e[0m",
		"\e[1;33mW\e[0m",
		"\e[1;32mN\e[0m",
		"\e[1;35mI\e[0m",
		"\e[1;36mD\e[0m"
	};
	__writer_stderr(prio, cookie, buf, len,
			priotab[prio & ULOG_PRIO_LEVEL_MASK]);
}

/* copy log messages to stderr */
static void __writer_stderr_wrapper(uint32_t prio, struct ulog_cookie *cookie,
				    const char *buf, int len)
{
	static const char * const priotab[8] = {
		" ", " ", "C", "E", "W", "N", "I", "D"
	};
	__writer_stderr(prio, cookie, buf, len,
			priotab[prio & ULOG_PRIO_LEVEL_MASK]);
}

static void __ctrl_init(void)
{
	ulog_write_func_t writer = __writer_null;
#ifndef _WIN32
	const char *prop, *dev;
	char devbuf[32];
	struct stat st;

	/* first try to use ulogger kernel device */
	dev = "/dev/" ULOGGER_LOG_MAIN;
	prop = getenv("ULOG_DEVICE");
	if (prop) {
		snprintf(devbuf, sizeof(devbuf), "/dev/ulog_%s", prop);
		dev = devbuf;
	}

	ctrl.fd = open(dev, O_WRONLY|O_CLOEXEC);
	if ((ctrl.fd >= 0) &&
			/* sanity check: /dev/ulog_* must be device files */
			((fstat(ctrl.fd, &st) < 0) || !S_ISCHR(st.st_mode))) {
		close(ctrl.fd);
		ctrl.fd = -1;
	}

	if (ctrl.fd >= 0)
		writer = __writer_kernel;
	else if (ulog_is_android())
		writer = ulog_writer_android;
	else
		writer = __writer_null;
#endif

	/* optionally output a copy of messages to stderr */
	if (getenv("ULOG_STDERR") || writer == __writer_null) {
		ctrl.writer2 = writer;
		writer = __writer_stderr_wrapper;
		if (getenv("ULOG_STDERR_COLOR"))
			writer = __writer_stderr_wrapper_color;
	}
	/* here we rely on the following assignment being atomic... */
	ctrl.writer = writer;
}

static void __writer_init(uint32_t prio, struct ulog_cookie *cookie,
			  const char *buf, int len)

{
	pthread_mutex_lock(&ctrl.lock);

	if (ctrl.writer == __writer_init)
		__ctrl_init();

	pthread_mutex_unlock(&ctrl.lock);

	ctrl.writer(prio, cookie, buf, len);
}

ULOG_EXPORT int ulog_set_write_func(ulog_write_func_t func)
{
	ulog_write_func_t writer;

	if (!func)
		return -EINVAL;

	pthread_mutex_lock(&ctrl.lock);

	/* optionally output a copy of messages to stderr */
	if (getenv("ULOG_STDERR")) {
		ctrl.writer2 = func;
		writer = __writer_stderr_wrapper;
		if (getenv("ULOG_STDERR_COLOR"))
			writer = __writer_stderr_wrapper_color;
	} else {
		writer = func;
	}

	/* here we rely on the following assignment being atomic... */
	ctrl.writer = writer;

	pthread_mutex_unlock(&ctrl.lock);
	return 0;
}

ULOG_EXPORT ulog_write_func_t ulog_get_write_func(void)
{
	ulog_write_func_t writer;

	pthread_mutex_lock(&ctrl.lock);

	if (ctrl.writer == __writer_init)
		__ctrl_init();

	if (!getenv("ULOG_STDERR"))
		writer = ctrl.writer;
	else
		writer = ctrl.writer2;

	pthread_mutex_unlock(&ctrl.lock);
	return writer;
}

ULOG_EXPORT int ulog_set_cookie_register_func(ulog_cookie_register_func_t func)
{
	if (!func)
		return -EINVAL;

	pthread_mutex_lock(&ctrl.lock);
	ctrl.cookie_register_hook = func;
	pthread_mutex_unlock(&ctrl.lock);
	return 0;
}

ULOG_EXPORT int ulog_foreach(
		void (*cb) (struct ulog_cookie *cookie, void *userdata),
		void *userdata)
{
	struct ulog_cookie *p;

	if (cb == NULL)
		return -EINVAL;
	/*
	 * We can safely traverse the list without holding ctrl.lock, as nodes
	 * are never deleted.
	 */
	pthread_mutex_lock(&ctrl.lock);
	p = ctrl.cookie_list;
	pthread_mutex_unlock(&ctrl.lock);

	while (p) {
		/* ignore the default cookie */
		if (p != &__ulog_default_cookie)
			cb(p, userdata);
		p = p->next;
	}

	return 0;
}

/* parse a log level description (letter or digit) */
static int parse_level(int c)
{
	int level;
	static const unsigned char tab['Z'-'A'+1] = {
		['C'-'A'] = ULOG_CRIT,
		['D'-'A'] = ULOG_DEBUG,
		['E'-'A'] = ULOG_ERR,
		['I'-'A'] = ULOG_INFO,
		['N'-'A'] = ULOG_NOTICE,
		['W'-'A'] = ULOG_WARN,
	};
	if (isdigit(c))
		level = c-'0';
	else if (isupper(c))
		level = tab[c-'A'];
	else
		level = 0;

	if (level > ULOG_DEBUG)
		level = ULOG_DEBUG;
	return level;
}

ULOG_EXPORT void ulog_init_cookie(struct ulog_cookie *cookie)
{
	char buf[80];
	int olderrno, level = -1;
	ulog_cookie_register_func_t cookie_register_hook;
	const char *prop;

	/* make sure we do not corrupt errno for %m glibc extension */
	olderrno = errno;

	if (cookie->name[0]) {
		/* get a tag-specific level */
		snprintf(buf, sizeof(buf), "ULOG_LEVEL_%s", cookie->name);
		/* in theory, getenv() is not reentrant */
		prop = getenv(buf);
		if (prop)
			/* coverity[tainted_data] */
			level = parse_level(prop[0]);
	}
	if (level < 0) {
		/* fallback to global level */
		prop = getenv("ULOG_LEVEL");
		if (prop)
			/* coverity[tainted_data] */
			level = parse_level(prop[0]);
	}
	if ((level < 0) && (__ulog_default_cookie.level >= 0))
		/* fallback to empty tag level */
		level = __ulog_default_cookie.level;

	if (level < 0)
		/* fallback to default level */
		level = ULOG_INFO;

	pthread_mutex_lock(&ctrl.lock);

	if (cookie->level < 0) {
		/* insert cookie in global linked list */
		cookie->next = ctrl.cookie_list;
		ctrl.cookie_list = cookie;
		/* insert barrier here? */
		cookie->level = level;
		cookie_register_hook = ctrl.cookie_register_hook;
	} else {
		/* cookie already registered */
		cookie_register_hook = NULL;
	}

	pthread_mutex_unlock(&ctrl.lock);

	/* call cookie register hook func */
	if (cookie_register_hook)
		cookie_register_hook(cookie);

	errno = olderrno;
}

ULOG_EXPORT void ulog_vlog_write(uint32_t prio, struct ulog_cookie *cookie,
				 const char *fmt, va_list ap)
{
	int ret;
	char buf[ULOG_BUF_SIZE];
	const int bufsize = (int)sizeof(buf);

	ret = vsnprintf(buf, bufsize, fmt, ap);
	if (ret >= bufsize)
		/* truncated output */
		ret = bufsize-1;
	if (ret >= 0)
		ctrl.writer(prio, cookie, buf, ret+1);
}

ULOG_EXPORT void ulog_log_write(uint32_t prio, struct ulog_cookie *cookie,
				const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ulog_vlog_write(prio, cookie, fmt, ap);
	va_end(ap);
}

ULOG_EXPORT void ulog_log_str(uint32_t prio, struct ulog_cookie *cookie,
			      const char *str)
{
	if (cookie->level < 0)
		ulog_init_cookie(cookie);

	if ((int)(prio & ULOG_PRIO_LEVEL_MASK) <= cookie->level)
		ctrl.writer(prio, cookie, str, strlen(str)+1);
}

ULOG_EXPORT void ulog_log_buf(uint32_t prio, struct ulog_cookie *cookie,
			      const void *data, int len)
{
	if (cookie->level < 0)
		ulog_init_cookie(cookie);

	if ((int)(prio & ULOG_PRIO_LEVEL_MASK) <= cookie->level)
		ctrl.writer(prio, cookie, data, len);
}

ULOG_EXPORT void ulog_init(struct ulog_cookie *cookie)
{
	/* make sure cookie is initialized */
	if (cookie->level < 0)
		ulog_init_cookie(cookie);
}

ULOG_EXPORT void ulog_set_level(struct ulog_cookie *cookie, int level)
{
	/* sanitize input */
	if (level < 0)
		level = 0;
	if (level > ULOG_DEBUG)
		level = ULOG_DEBUG;

	ulog_init(cookie);

	/* this last assignment is racy, but in a harmless way */
	cookie->level = level;
}

ULOG_EXPORT int ulog_get_level(struct ulog_cookie *cookie)
{
	ulog_init(cookie);
	return cookie->level;
}

ULOG_EXPORT int ulog_set_tag_level(const char *name, int level)
{
	int ret = -1;
	struct ulog_cookie *p, *cookie = NULL;

	/* lookup tag by name */
	pthread_mutex_lock(&ctrl.lock);

	for (p = ctrl.cookie_list; p; p = p->next)
		if (strcmp(p->name, name) == 0) {
			cookie = p;
			break;
		}

	pthread_mutex_unlock(&ctrl.lock);

	if (cookie) {
		ulog_set_level(cookie, level);
		ret = 0;
	}
	return ret;
}

ULOG_EXPORT int ulog_get_tag_names(const char **nametab, int maxlen)
{
	int idx = 0;
	const struct ulog_cookie *p;

	pthread_mutex_lock(&ctrl.lock);

	for (p = ctrl.cookie_list; p && (idx < maxlen); p = p->next)
		nametab[idx++] = p->name;

	pthread_mutex_unlock(&ctrl.lock);

	return idx;
}

ULOG_EXPORT int ulog_get_time_monotonic(unsigned long long *now_ms)
{
	struct timespec __tp;

	if (!now_ms)
		return -EINVAL;

	(void)clock_gettime(CLOCK_MONOTONIC, &__tp);
	*now_ms =  __tp.tv_sec * 1000ULL + __tp.tv_nsec / 1000000ULL;

	return 0;
}
