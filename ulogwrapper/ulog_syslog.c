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

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <ulog.h>
#include <stdio.h>

static int init_done;
static int allow_long_logs;
static pthread_mutex_t ulog_lock = PTHREAD_MUTEX_INITIALIZER;
static struct ulog_cookie cookie = {
	.name     = "",
	.namesize = 1,
	.level    = -1,
	.next     = NULL,
};

/* codecheck_ignore[AVOID_EXTERNS] */
void openlog(const char *ident, int option, int facility);
/* codecheck_ignore[AVOID_EXTERNS] */
void syslog(int priority, const char *format, ...);
/* codecheck_ignore[AVOID_EXTERNS] */
void closelog(void);
/* codecheck_ignore[AVOID_EXTERNS] */
void vsyslog(int priority, const char *format, va_list ap);
/* codecheck_ignore[AVOID_EXTERNS] */
int setlogmask(int mask);

/* we need to define this if FORTIFY is used */
/* codecheck_ignore[AVOID_EXTERNS] */
void __syslog_chk(int priority, int flag, const char *format, ...);
/* codecheck_ignore[AVOID_EXTERNS] */
void __vsyslog_chk(int priority, int flag, const char *format, va_list ap);

/* facility and some options are ignored */
void openlog(const char *ident,
	     int option,
	     int facility __attribute__((unused)))
{
	if (!init_done) {
		pthread_mutex_lock(&ulog_lock);
		if (!init_done) {
			if (ident) {
				cookie.name = strdup(ident);
				cookie.namesize = strlen(ident)+1;
			}
			if (getenv("ULOGWRAPPER_LONG_LOGS"))
				allow_long_logs = 1;
			init_done = 1;
		}
		pthread_mutex_unlock(&ulog_lock);
	}

	/* add a log to force device opening if LOG_NDELAY option given */
	if (option & 0x08)
		ulog_log(ULOG_INFO, &cookie, "redirecting syslog to ulog");
}

static void ulog_vlog_notruncate(uint32_t prio, struct ulog_cookie *cookie,
				 const char *fmt, va_list ap)
{
	int ret;
	char buf[ULOG_BUF_SIZE];
	const int bufsize = (int)sizeof(buf);

	ret = vsnprintf(buf, bufsize, fmt, ap);
	if (ret < 0)
		return;
	if (ret < bufsize) {
		ulog_log_buf(prio, cookie, buf, ret+1);
		return;
	}
	if (!allow_long_logs) {
		ulog_log_buf(prio, cookie, buf, bufsize);
		return;
	}

	const int mbufsize = ret+1;
	char *mbuf = malloc(mbufsize);
	if (!mbuf)
		return;
	vsprintf(mbuf, fmt, ap);
	ulog_log_buf(prio, cookie, mbuf, mbufsize);
	free(mbuf);
}

__attribute__ ((format (printf, 2, 3)))
void syslog(int priority, const char *format, ...)
{
	va_list ap;

	if (!init_done)
		openlog(NULL, 0, 0);

	va_start(ap, format);
	ulog_vlog_notruncate(
		priority & ULOG_PRIO_LEVEL_MASK, &cookie, format, ap);
	va_end(ap);
}

void closelog(void)
{
}

__attribute__ ((format (printf, 2, 0)))
void vsyslog(int priority, const char *format, va_list ap)
{
	if (!init_done)
		openlog(NULL, 0, 0);

	ulog_vlog_notruncate(
		priority & ULOG_PRIO_LEVEL_MASK, &cookie, format, ap);
}

__attribute__ ((format (printf, 3, 4)))
void __syslog_chk(int priority,
		  int flag __attribute__((unused)),
		  const char *format, ...)
{
	va_list ap;

	if (!init_done)
		openlog(NULL, 0, 0);

	va_start(ap, format);
	ulog_vlog_notruncate(
		priority & ULOG_PRIO_LEVEL_MASK, &cookie, format, ap);
	va_end(ap);
}

__attribute__ ((format (printf, 3, 0)))
void __vsyslog_chk(int priority,
		   int flag __attribute__((unused)),
		   const char *format, va_list ap)
{
	if (!init_done)
		openlog(NULL, 0, 0);

	ulog_vlog_notruncate(
		priority & ULOG_PRIO_LEVEL_MASK, &cookie, format, ap);
}

int setlogmask(int mask)
{
	int level = -1;

	if (!init_done)
		openlog(NULL, 0, 0);

	while (mask) {
		level++;
		mask = (uint32_t)mask >> 1;
	}

	mask = (1 << (cookie.level+1))-1;

	/* sanitize input */
	if (level < ULOG_CRIT)
		level = ULOG_CRIT;
	else if (level > ULOG_DEBUG)
		level = ULOG_DEBUG;

	ulog_set_level(&cookie, level);

	return mask;
}
