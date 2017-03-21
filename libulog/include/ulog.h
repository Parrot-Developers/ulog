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
 * libulog: a minimalistic logging library derived from Android logger
 *
 */

#ifndef _PARROT_ULOG_H
#define _PARROT_ULOG_H

/*
 * HOW TO USE ULOG:
 * ----------------
 *
 * 1. Declare one or several ULOG tag names in a .c or .cpp source file, like
 * this:
 *
 *   #include "ulog.h"
 *   ULOG_DECLARE_TAG(toto);
 *   ULOG_DECLARE_TAG(Foo_Bar);
 *
 * Note that the argument of ULOG_DECLARE_TAG() is the tag name and should be a
 * valid C symbol string, such as 'my_module', 'MyTag', etc.
 *
 * 2. Then, set the default tag to use inside a given source file by defining
 * macro ULOG_TAG before including "ulog.h":
 *
 *   #define ULOG_TAG Foo_Bar
 *   #include "ulog.h"
 *
 * 3. You can now use short macros for logging:
 *
 *   ULOGW("This module will auto-destruct in %d seconds...\n", 3);
 *   ULOGE("Fatal error\n");
 *
 * If you forget to define macro ULOG_TAG, then a default empty tag is used.
 *
 * NOTE: If you need to log messages from a signal handler, make sure a first
 * message using the tag is logged at runtime before installing your handler.
 *
 * HOW TO CONTROL ULOG LOGGING LEVEL:
 * ----------------------------------
 * ULOG logging is globally controlled by environment variable ULOG_LEVEL.
 * This variable should contain a single letter ('C', 'E', 'W', 'N', 'I', or
 * 'D') or, alternatively, a single digit with an equivalent meaning:
 *
 * C = Critical = 2
 * E = Error    = 3
 * W = Warning  = 4
 * N = Notice   = 5
 * I = Info     = 6
 * D = Debug    = 7
 *
 * For instance, to enable all priorities up to and including the 'Warning'
 * level, you should set:
 * ULOG_LEVEL=W
 * or, equivalently,
 * ULOG_LEVEL=4
 *
 * The default logging level is 'I', i.e. all priorities logged except Debug.
 * ULOG_LEVEL controls logging levels globally for all tags.
 * Setting an empty ULOG_LEVEL string disables logging completely.
 * You can also control the logging level of a specific tag by defining
 * environment variable ULOG_LEVEL_<tagname>. For instance:
 *
 * ULOG_LEVEL_Foo_Bar=D   (set level Debug for tag 'Foo_Bar')
 *
 * The above environment variables are read only once, before the first use of
 * a tag. To dynamically change a logging level at any time, you can use macro
 * ULOG_SET_LEVEL() like this:
 *
 *   ULOG_SET_LEVEL(ULOG_DEBUG);
 *   ULOGD("This debug message will be logged.");
 *   ULOG_SET_LEVEL(ULOG_INFO);
 *   ULOGD("This debug message will _not_ be logged.");
 *   ULOGI("But this one will be.");
 *
 * ULOG_SET_LEVEL() takes precedence over ULOG_LEVEL_xxx environment variables.
 * If ULOG_SET_LEVEL() is used without a defined ULOG_TAG, then it sets the
 * default logging level used when no environment variable is defined.
 * ULOG_GET_LEVEL() returns the current logging level of the default tag defined
 * with ULOG_TAG.
 *
 * If you need to dynamically control the logging level of an external tag, i.e.
 * a tag not declared in your code (for instance declared and used in a library
 * to which your code is linked), you can use the following function (assuming
 * the tag is 'foobar'):
 *
 *   ulog_set_tag_level("foobar", ULOG_WARN);
 *
 * There is a restriction to the above code: the tag will be accessible and
 * controllable at runtime only after its has been used at least once. This is
 * because the tag "registers" itself during its first use, and remains unknown
 * until it does so. A library can make sure its tags are externally visible
 * by forcing early tag registration with macro ULOG_INIT() like this:
 *
 *   ULOG_INIT(foobar);
 *   // at this point, tag 'foobar' logging level is externally controllable
 *
 * This can be done typically during library initialization.
 * You can also dynamically list 'registered' tags at runtime with function
 * ulog_get_tag_names().
 *
 * HOW TO CONTROL ULOG OUTPUT DEVICE
 * ---------------------------------
 * To control which kernel logging device is used, use environment variable
 * ULOG_DEVICE; for instance:
 *
 * ULOG_DEVICE=balboa  (default device is 'main')
 *
 * To enable printing a copy of each message to stderr:
 * ULOG_STDERR=y
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

/*----------------------------------------------------------------------------*/
/* ULOG API */

/**
 * Declare a tag.
 *
 * Tag name should be a valid C symbol name.
 */
#define ULOG_DECLARE_TAG(name)     __ULOG_DECLARE_TAG(name)

/**
 * ULOG priority levels: keep compatibility with a subset of syslog levels
 */
#define ULOG_CRIT    2       /* critical conditions */
#define ULOG_ERR     3       /* error conditions */
#define ULOG_WARN    4       /* warning conditions */
#define ULOG_NOTICE  5       /* normal but significant condition */
#define ULOG_INFO    6       /* informational message */
#define ULOG_DEBUG   7       /* debug-level message */

/**
 * Simple logging macros with implicit tag.
 *
 * You should first define ULOG_TAG and include "ulog.h" before using these
 * macros.
 */
#define ULOGC(...)      ULOG_PRI(ULOG_CRIT,   __VA_ARGS__)
#define ULOGE(...)      ULOG_PRI(ULOG_ERR,    __VA_ARGS__)
#define ULOGW(...)      ULOG_PRI(ULOG_WARN,   __VA_ARGS__)
#define ULOGN(...)      ULOG_PRI(ULOG_NOTICE, __VA_ARGS__)
#define ULOGI(...)      ULOG_PRI(ULOG_INFO,   __VA_ARGS__)
#define ULOGD(...)      ULOG_PRI(ULOG_DEBUG,  __VA_ARGS__)

/**
 * Log a simple string message and errno with priority ERR.
 */
#define ULOG_ERRNO(_msg, _err)						\
	ULOGE("%s:%d: %s err=%d(%s)", __func__, __LINE__, _msg, _err,	\
		      strerror(_err))

/**
 * Maximum length of an ascii message.
 *
 * Non-binary messages exceeding this length will be truncated.
 */
#define ULOG_BUF_SIZE   256

/*----------------------------------------------------------------------------*/
/* Additional API for dynamically controlling logging levels */

/**
 * Set logging level of current implicit tag (as defined with ULOG_TAG).
 *
 * @param level: one of ULOG_CRIT, ULOG_ERR, ULOG_WARN, etc.
 */
#define ULOG_SET_LEVEL(level)        ulog_set_level(&__ULOG_COOKIE, level)

/**
 * Get current logging level of current implicit tag (as defined with ULOG_TAG).
 *
 * @return level: one of ULOG_CRIT, ULOG_ERR, ULOG_WARN, etc.
 */
#define ULOG_GET_LEVEL()             ulog_get_level(&__ULOG_COOKIE)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Select tag by name and set its logging level.
 *
 * Note that this function will find a given tag at runtime only after its first
 * use in a message. See also macro ULOG_INIT().
 *
 * @param name  Tag name
 * @param level Logging level: one of ULOG_CRIT, ULOG_ERR, ULOG_WARN, etc.
 * @return      0 if successful, -1 if tag was not found
 */
int ulog_set_tag_level(const char *name, int level);

/**
 * Output currently known tag names.
 *
 * Note that this function will list a given tag at runtime only after its first
 * use in a message. See also macro ULOG_INIT().
 *
 * @param names  An array of name pointers allocated by the caller
 * @param maxlen The size of @ref names
 * @return       The number of names written to @ref names (<= @ref maxlen)
 */
int ulog_get_tag_names(const char **names, int maxlen);

#ifdef __cplusplus
}
#endif

/**
 * Force global registration of a tag before its first use.
 *
 * Normally, a tag becomes globally registered and searchable by name only
 * after its first use in a log message. This macro forces an immediate
 * registration. This can be useful if a logging level must be set by name
 * before the tag has ever been used.
 */
#define ULOG_INIT(name)              ulog_init(&__ULOG_REF(name))

/*----------------------------------------------------------------------------*/
/* Misc macros; useful for building custom logging macros */

#define ULOG_PRI(_prio, ...)        ulog_log(_prio, &__ULOG_COOKIE, __VA_ARGS__)
#define ULOG_PRI_VA(_prio, _f, _a)  ulog_vlog(_prio, &__ULOG_COOKIE, _f, _a)
#define ULOG_STR(_prio, _str)       ulog_log_str(_prio, &__ULOG_COOKIE, _str)
#define ULOG_BUF(_prio, _d, _sz)    ulog_log_buf(_prio, &__ULOG_COOKIE, _d, _sz)
#define ULOG_BIN(_prio, _dat, _sz)  \
	ULOG_BUF(((_prio)|(1U << ULOG_PRIO_BINARY_SHIFT)), _dat, _sz)

/* Priority format */
#define ULOG_PRIO_LEVEL_MASK         0x7
#define ULOG_PRIO_BINARY_SHIFT       7
#define ULOG_PRIO_COLOR_SHIFT        8

/*----------------------------------------------------------------------------*/
/* Internal stuff; no serviceable part below */

#define __ULOG_REF(_name)  __ulog_cookie_ ## _name
#define __ULOG_REF2(_name) __ULOG_REF(_name)
#define __ULOG_DECL(_n)    struct ulog_cookie __ULOG_REF(_n) =	\
	{#_n, sizeof(#_n), -1, NULL, NULL}

#ifdef __cplusplus
/* codecheck_ignore[STORAGE_CLASS] */
#define __ULOG_DECLARE_TAG(_nam)  extern "C" { __ULOG_DECL(_nam); }
/* codecheck_ignore[STORAGE_CLASS] */
#define __ULOG_USE_TAG(_nam)      extern "C" struct ulog_cookie __ULOG_REF(_nam)
#else
#define __ULOG_DECLARE_TAG(_name) __ULOG_DECL(_name)
/* codecheck_ignore[STORAGE_CLASS] */
#define __ULOG_USE_TAG(_name)     extern struct ulog_cookie __ULOG_REF(_name)
#endif

#ifndef ULOG_TAG
#define __ULOG_COOKIE __ulog_default_cookie
#else
__ULOG_USE_TAG(ULOG_TAG);
#define __ULOG_COOKIE __ULOG_REF2(ULOG_TAG)
#endif

#ifdef __cplusplus
extern "C" {
#endif
struct ulog_cookie {
	const char         *name;     /* tag name */
	int                 namesize; /* tag name length + 1 */
	int                 level;    /* current logging level for this tag */
	void               *userdata; /* cookie userdata */
	struct ulog_cookie *next;     /* next registered cookie */
};

extern struct ulog_cookie __ulog_default_cookie;

#if !defined(UNLIKELY)
#define UNLIKELY(x)	__builtin_expect(!!(x), 0)
#endif

void ulog_init_cookie(struct ulog_cookie *cookie);
void ulog_vlog_write(uint32_t prio, struct ulog_cookie *cookie,
		     const char *fmt, va_list ap)
	__attribute__ ((format (printf, 3, 0)));

void ulog_log_write(uint32_t prio, struct ulog_cookie *cookie,
		    const char *fmt, ...)
	__attribute__ ((format (printf, 3, 4)));

/* force inlining of priority filtering for better performance */
#define ulog_vlog(_prio, _cookie, _fmt, _ap)				\
	do {								\
		uint32_t __p = (_prio);					\
		if (UNLIKELY((_cookie)->level < 0))			\
			ulog_init_cookie((_cookie));			\
		if ((int)(__p & ULOG_PRIO_LEVEL_MASK) <=		\
				(_cookie)->level)			\
			ulog_vlog_write(__p, (_cookie), _fmt, _ap);	\
	} while (0)

/* force inlining of priority filtering for better performance
 * Note: gcc will not inline the code if it is in a function because of the
 *       variable number of arguments.
 * Note: the _cookie is evaluated several times. Putting it in a local variable
 *       causes misleading messages depending on gcc version used */
#define ulog_log(_prio, _cookie, ...)					\
	do {								\
		uint32_t __p = (_prio);					\
		if (UNLIKELY((_cookie)->level < 0))			\
			ulog_init_cookie((_cookie));			\
		if ((int)(__p & ULOG_PRIO_LEVEL_MASK) <=		\
				(_cookie)->level)			\
			ulog_log_write(__p, (_cookie), __VA_ARGS__);	\
	} while (0)

void ulog_log_buf(uint32_t prio, struct ulog_cookie *cookie, const void *buf,
		  int size);
void ulog_log_str(uint32_t prio, struct ulog_cookie *cookie, const char *str);
void ulog_init(struct ulog_cookie *cookie);
void ulog_set_level(struct ulog_cookie *cookie, int level);
int ulog_get_level(struct ulog_cookie *cookie);

typedef void (*ulog_write_func_t) (uint32_t prio, struct ulog_cookie *cookie,
		  const char *buf, int len);

int ulog_set_write_func(ulog_write_func_t func);

typedef void (*ulog_cookie_register_func_t) (struct ulog_cookie *cookie);

int ulog_set_cookie_register_func(ulog_cookie_register_func_t func);

/**
 * Call a function for each cookie.
 *
 * @warning The cookie list is locked and
 * cannot be modified during the callback.
 *
 * @param cb The callback.
 * @param userdata user data.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @note Functions supported in the callback are:
 *   - ulog_get_level
 *   - ulog_set_level
 *   - ulog_log
 *   - ulog_vlog
 *   - ulog_log_buf
 *   - ulog_log_str
 * The cookie parameter of these functions must come from the callback argument.
 */
int ulog_foreach(void (*cb) (struct ulog_cookie *cookie, void *userdata),
		void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* _PARROT_ULOG_H */
