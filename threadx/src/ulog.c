/**
 * Copyright (C) 2017 Parrot S.A.
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
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#include "AmbaDataType.h"
#include "AmbaPrint.h"
#include "AmbaKAL.h"
#include "AmbaUtility.h"
#include "ulog.h"

#define ULOG_EXPORT

#define ULOG_DEFAULT_COOKIE_NAME "threadx"
#define ULOG_DEFAULT_COOKIE_LEVEL ULOG_INFO

/* this cookie is used when ULOG_TAG is undefined */
struct ulog_cookie __ulog_default_cookie = {
	.name     = ULOG_DEFAULT_COOKIE_NAME,
	.namesize = sizeof(ULOG_DEFAULT_COOKIE_NAME),
	.level    = ULOG_DEFAULT_COOKIE_LEVEL,
	.next     = NULL,
};

static struct {
	bool ulog_ready;
	AMBA_KAL_MUTEX_t lock;
	struct ulog_cookie *cookie_list;
} ctrl = {
	.ulog_ready = false,
	.cookie_list = &__ulog_default_cookie,
};

ULOG_EXPORT int ulog_set_write_func(ulog_write_func_t func)
{
    return -ENOSYS;
}

ULOG_EXPORT int ulog_set_cookie_register_func(ulog_cookie_register_func_t func)
{
    return -ENOSYS;
}

ULOG_EXPORT int ulog_foreach(
		void (*cb) (struct ulog_cookie *cookie, void *userdata),
		void *userdata)
{
	struct ulog_cookie *p;

	if (!ctrl.ulog_ready)
		return -EPERM;

	if (cb == NULL)
		return -EINVAL;

	if (AmbaKAL_MutexTake(&ctrl.lock, AMBA_KAL_WAIT_FOREVER) != OK)
		return -EPERM;  /* Can't take mutex */

	for (p = ctrl.cookie_list; p; p = p->next)
		cb(p, userdata);

	(void)AmbaKAL_MutexGive(&ctrl.lock);

	return 0;
}

/* parse a log level description (letter or digit) */
int parse_level(int c)
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
	int olderrno, level = -1;

	if (!ctrl.ulog_ready)
		return;

	/* make sure we do not corrupt errno for %m glibc extension */
	olderrno = errno;

	if (cookie->name[0]) {
		/* get a tag-specific level */
		/* TODO find a preconfigured default value for the log */
	}
	if (level < 0) {
		/* fallback to global level */
		/* TODO find a global default value */
	}
	if ((level < 0) && (__ulog_default_cookie.level >= 0))
		/* fallback to empty tag level */
		level = __ulog_default_cookie.level;

	if (level < 0)
		/* fallback to default level */
		level = ULOG_INFO;

	if (AmbaKAL_MutexTake(&ctrl.lock, AMBA_KAL_WAIT_FOREVER) != OK)
		return;  /* Can't take mutex */

	if (cookie->level < 0) {
		/* insert cookie in global linked list */
		cookie->next = ctrl.cookie_list;
		ctrl.cookie_list = cookie;
		/* insert barrier here? */
		cookie->level = level;
	}

	(void)AmbaKAL_MutexGive(&ctrl.lock);

	errno = olderrno;
}

#ifndef CONFIG_PARROT_LINUXLOG
static void AmbaLog(int prio, const char *tag, int tagsize, const char *fmt,
								va_list ap)
{
	char buf[ULOG_BUF_SIZE];
	const int bufsize = (int)sizeof(buf);
	int ret;
	struct {
		char marker;
		int color;
	} prio_tab[] = {
		[ULOG_CRIT]   = {'C', CYAN},
		[ULOG_ERR]    = {'E', RED},
		[ULOG_WARN]   = {'W', YELLOW},
		[ULOG_NOTICE] = {'N', MAGENTA},
		[ULOG_INFO]   = {'I', -1},
		[ULOG_DEBUG]  = {'D', GRAY},
	};

	ret = vsnprintf(buf, bufsize, fmt, ap);
	if (ret >= bufsize)
		/* truncated output */
		ret = bufsize - 1;

	if (ret >= 0 && prio_tab[prio].color == -1)
		AmbaPrint("[%c] %s: %s", prio_tab[prio].marker, tag, buf);
	else if (ret >= 0)
		AmbaPrintColor(prio_tab[prio].color, "[%c] %s: %s",
			       prio_tab[prio].marker, tag, buf);
}
#endif

ULOG_EXPORT void ulog_vlog_write(uint32_t prio, struct ulog_cookie *cookie,
				 const char *fmt, va_list ap)
{
	if (!ctrl.ulog_ready)
		return;

	AmbaLog(prio, cookie->name, cookie->namesize, fmt, ap);
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
}

ULOG_EXPORT void ulog_log_buf(uint32_t prio, struct ulog_cookie *cookie,
			      const void *data, int len)
{
}

ULOG_EXPORT void ulog_init(struct ulog_cookie *cookie)
{
	if (!ctrl.ulog_ready)
		return;

	/* make sure cookie is initialized */
	if (cookie->level < 0)
		ulog_init_cookie(cookie);
}

ULOG_EXPORT void ulog_set_level(struct ulog_cookie *cookie, int level)
{
	if (!ctrl.ulog_ready)
		return;

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
	if (!ctrl.ulog_ready)
		return -EPERM;

	ulog_init(cookie);
	return cookie->level;
}

ULOG_EXPORT int ulog_set_tag_level(const char *name, int level)
{
	int ret = -EPERM;
	struct ulog_cookie *p, *cookie = NULL;

	if (!ctrl.ulog_ready)
		return ret;

	/* lookup tag by name */
	if (AmbaKAL_MutexTake(&ctrl.lock, AMBA_KAL_WAIT_FOREVER) != OK)
		return ret;  /* Can't take mutex */

	for (p = ctrl.cookie_list; p; p = p->next) {
		if (strcmp(p->name, name) == 0) {
			cookie = p;
			break;
		}
	}

	(void)AmbaKAL_MutexGive(&ctrl.lock);

	if (cookie) {
		ulog_set_level(cookie, level);
		ret = 0;
	}
	return ret;
}

ULOG_EXPORT int ulog_get_tag_level(const char *name)
{
	int ret = -1;
	struct ulog_cookie *p;

	if (!ctrl.ulog_ready)
		return ret;

	/* lookup tag by name */
	if (AmbaKAL_MutexTake(&ctrl.lock, AMBA_KAL_WAIT_FOREVER) != OK)
		return ret; /* Can't take mutex */

	for (p = ctrl.cookie_list; p; p = p->next) {
		if (strcmp(p->name, name) == 0) {
			ret = p->level;
			break;
		}
	}

	(void)AmbaKAL_MutexGive(&ctrl.lock);

	return ret;
}

ULOG_EXPORT int ulog_get_tag_names(const char **nametab, int maxlen)
{
	int idx = 0;
	const struct ulog_cookie *p;

	if (!ctrl.ulog_ready)
		return idx;

	if (AmbaKAL_MutexTake(&ctrl.lock, AMBA_KAL_WAIT_FOREVER) != OK)
		return idx; /* Can't take mutex */

	for (p = ctrl.cookie_list; p && (idx < maxlen); p = p->next)
		nametab[idx++] = p->name;

	(void)AmbaKAL_MutexGive(&ctrl.lock);

	return idx;
}

ULOG_EXPORT int ulog_get_time_monotonic(unsigned long long *now_ms)
{
	UINT64 *__now = now_ms;

	if (!now_ms)
		return -EINVAL;

	AmbaUtility_GetHighResolutionTimeStamp(__now);
	return 0;
}

void ulog_amba_early_init(void)
{
	/* Threadx mutex can't be created statically,
	 * they need to be created before any thread try to use it. */
	if (AmbaKAL_MutexCreate(&ctrl.lock) != OK) {
		AmbaPrint("failed to create mutex for ulog");
		return;
	}

	ctrl.ulog_ready = true;
}


char ulog_prio2char(int prio)
{
	static const char priotab[8] = {
		[0]           = ' ',
		[1]           = ' ',
		[ULOG_CRIT]   = 'C',
		[ULOG_ERR]    = 'E',
		[ULOG_WARN]   = 'W',
		[ULOG_NOTICE] = 'N',
		[ULOG_INFO]   = 'I',
		[ULOG_DEBUG]  = 'D'
	};

	if (prio < 0 || prio > ULOG_DEBUG)
		return ' ';

	return priotab[prio];
}
