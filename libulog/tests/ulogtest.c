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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

#define  ULOG_TAG pulsarsoca
#include "ulog.h"
#include "ulograw.h"

#ifdef __linux__
#include <sys/prctl.h>
static void set_thread_name(const char *name)
{
	(void)prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
}
#else
static void set_thread_name(const char *name) {}
#endif /* __linux__ */


ULOG_DECLARE_TAG(pulsarsoca);
ULOG_DECLARE_TAG(My_Program);
ULOG_DECLARE_TAG(z);
ULOG_DECLARE_TAG(a_very_lonnnnnnnnnnnnnnnnnnng_tag);

__ULOG_USE_TAG(My_Program);
__ULOG_USE_TAG(z);
__ULOG_USE_TAG(a_very_lonnnnnnnnnnnnnnnnnnng_tag);

/* macro with explicit tag */
#define UTLOG(_t, ...) ulog_log(ULOG_INFO, &__ULOG_REF(_t), __VA_ARGS__)

static void test_levels(void)
{
	ULOGC("Level C");
	ULOGE("Level E");
	ULOGW("Level W");
	ULOGN("Level N");
	ULOGI("Level I");
	ULOGD("Level D");
	ULOG_PRI(ULOG_CRIT, "Level C");
	ULOG_PRI(ULOG_ERR, "Level E");
	ULOG_PRI(ULOG_WARN, "Level W");
	ULOG_PRI(ULOG_NOTICE, "Level N");
	ULOG_PRI(ULOG_INFO, "Level I");
	ULOG_PRI(ULOG_DEBUG, "Level D");
}

static void test_longline(void)
{
#define _L "All work and no play makes Jack a dull boy\n"
/* codecheck_ignore[COMPLEX_MACRO] */
#define LINE _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L
	char msg[] = LINE;
#define _M "Machete don't text."
/* codecheck_ignore[COMPLEX_MACRO] */
#define _N _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M _M
/* codecheck_ignore[COMPLEX_MACRO] */
#define _O _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N _N
	char msg2[] = _O;
	ULOGN(msg);
	ULOG_BUF(ULOG_CRIT, msg, sizeof(msg));
	ULOG_BIN(ULOG_CRIT, msg, sizeof(msg));
	ULOG_STR(ULOG_CRIT, msg2);
}

static void test_tag_length(void)
{
	UTLOG(z, "using a short tag\n");
	UTLOG(a_very_lonnnnnnnnnnnnnnnnnnng_tag, "using a long tag\n");
}

static void test_multiline(void)
{
	ULOGE("This is line1,\nline 2...\n...and line 3.\n");
}

static void test_binary(void)
{
	char buf[] = "Binary data \001 \002 \003 \077";
	ULOG_BIN(ULOG_CRIT, buf, sizeof(buf));
}


static void test_dyn_level(void)
{
	int ret;

	ret = ulog_set_tag_level("pulsarsoca", ULOG_ERR);
	ULOGC("ulog_set_tag_level returned %d", ret);
	ULOGW("This warning should not appear");

	ret = ulog_set_tag_level("pulsarsoca", ULOG_DEBUG);
	ULOGI("ulog_set_tag_level returned %d", ret);
	ULOGW("This warning should appear");

	ret = ulog_set_tag_level("xxx", ULOG_DEBUG);
	ULOGI("ulog_set_tag_level(xxx) returned %d", ret);
}

static void test_args(void)
{
	ULOGI("x=%d, y=%s, z=%c, t=%x", 3, "hello", 'A', 32);
}

static void test_get_tags(void)
{
	int i, ret;
	const char *tab[16];

	ret = ulog_get_tag_names(tab, 16);
	ULOGI("ulog_get_tag_names returned %d", ret);
	if (ret >= 0)
		for (i = 0; i < ret; i++)
			ULOGI("tag #%d: '%s'", i, tab[i]);
}

static void test_color(void)
{
	const uint32_t color = 0xffeedd;

	ULOG_PRI((color << ULOG_PRIO_COLOR_SHIFT)|ULOG_WARN,
		 "This tag has color #%06x", color);
}

static void *thread_routine(void *arg)
{
	int id = (int)(intptr_t)arg;
	char name[16];

	snprintf(name, sizeof(name), "test_thread_%d", id);
	set_thread_name(name);
	ULOGI("message from thread #%d", id);

	return NULL;
}

static void test_threads(void)
{
	int i, ret;
#define NUM_THREADS 12
	pthread_t tid[NUM_THREADS];

	for (i = 0; i < NUM_THREADS; i++) {
		ret = pthread_create(&tid[i], NULL, &thread_routine,
				     (void *)(intptr_t)i);
		assert(ret == 0);
	}
	for (i = 0; i < NUM_THREADS; i++) {
		ret = pthread_join(tid[i], NULL);
		assert(ret == 0);
	}
}

/* custom logging function using va_list macro */
static void mylog(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ULOG_PRI_VA(ULOG_INFO, fmt, ap);
	va_end(ap);
}

static void test_va(void)
{
	mylog("x=%d y=%c z=%s", 42, 42, "42");
}

static void custom_write_func(uint32_t prio, struct ulog_cookie *cookie,
		  const char *buf, int len)
{
	static const char * const priotab[8] = {
		" ", " ", "C", "E", "W", "N", "I", "D"
	};

	fprintf(stderr, "%s: [%s] %s: %s\n", __func__, priotab[prio],
			cookie->name, buf);
}

static void test_custom_write_func(void)
{
	ulog_set_write_func(&custom_write_func);
	ULOGI("test_custom_write_func");
}

static void test_raw_mode(void)
{
	int fd, ret;
	struct timeval tv;
	struct ulog_raw_entry raw;

	fd = ulog_raw_open(NULL);
	if (fd < 0) {
		fprintf(stderr, "cannot open ulog in raw mode: %s\n",
			strerror(-fd));
		goto finish;
	}

	ret = gettimeofday(&tv, NULL);
	if (ret < 0) {
		perror("gettimeofday");
		goto finish;
	}

	/* random values for testing */
	raw.entry.len = 0; /* will be overwritten by kernel */
	raw.entry.hdr_size = 0; /* will be overwritten by kernel */
	raw.entry.pid = 42;
	raw.entry.tid = 44;
	raw.entry.sec = tv.tv_sec+1;
	raw.entry.nsec = tv.tv_usec*1000;
	raw.entry.euid = 1000;
	raw.prio = ULOG_WARN;
	raw.pname = "raw-process";
	raw.pname_len = strlen(raw.pname)+1;
	raw.tname = "raw-thread";
	raw.tname_len = strlen(raw.tname)+1;
	raw.tag = "raw-tag";
	raw.tag_len = strlen(raw.tag)+1;
	raw.message = "A first raw log message.";
	raw.message_len = strlen(raw.message)+1;

	ret = ulog_raw_log(fd, &raw);
	if (ret < 0) {
		fprintf(stderr, "cannog log raw entry: %s\n", strerror(-ret));
		goto finish;
	}

	/* random values for testing */
	raw.entry.len = 0; /* will be overwritten by kernel */
	raw.entry.hdr_size = 0; /* will be overwritten by kernel */
	raw.entry.pid = 0;
	raw.entry.tid = 0;
	raw.entry.sec = tv.tv_sec+2;
	raw.entry.nsec = tv.tv_usec*1000;
	raw.entry.euid = 1000;
	raw.prio = ULOG_INFO;
	raw.pname = "raw-process2";
	raw.pname_len = strlen(raw.pname)+1;
	raw.tname = "raw-thread2"; /* this should be ignored */
	raw.tname_len = strlen(raw.tname)+1;
	raw.tag = "raw-tag2";
	raw.tag_len = strlen(raw.tag)+1;
	raw.message = "A second raw log message.";
	raw.message_len = strlen(raw.message)+1;

	ret = ulog_raw_log(fd, &raw);
	if (ret < 0) {
		fprintf(stderr, "cannog log raw entry: %s\n", strerror(-ret));
		goto finish;
	}

finish:
	if (fd >= 0)
		ulog_raw_close(fd);
}

static void test_throttling(void)
{
	int i;

	/* flood log buffer @ 200 Hz */
	for (i = 0; i < 100; i++) {
		ULOGI_THROTTLE(100, "I'm flooding the buffer and I know it");
		ULOGN_THROTTLE(200, "Throttling #%d\n", i);
		usleep(5000);
	}
}

static void test_change(void)
{
	int i;

	for (i = 0; i < 10; i++) {
		ULOGI_CHANGE(i,   "i=%2d", i);
		ULOGW_CHANGE(i/2, "i=%2d: i/2 = %d", i, i/2);
		ULOGN_CHANGE(i/3, "i=%2d: i/3 = %d", i, i/3);
	}
}

static void *thread_change_routine(void *arg)
{
	const int delay = 100000;
	int i, id = (int)(intptr_t)arg;

	if (id == 0)
		usleep(delay/2);

	for (i = 0; i < 10; i++) {
		ULOGI_CHANGE(i, "thread #%d: value=%d", id, i);
		usleep(delay);
	}

	return NULL;
}

/* Run a test with threads to make sure TLS is effective */
static void test_change_threads(void)
{
	int i, ret;
	pthread_t tid[2];

	for (i = 0; i < 2; i++) {
		ret = pthread_create(&tid[i], NULL, &thread_change_routine,
				     (void *)(intptr_t)i);
		assert(ret == 0);
	}
	for (i = 0; i < 2; i++) {
		ret = pthread_join(tid[i], NULL);
		assert(ret == 0);
	}
}

int main(void)
{
	test_levels();
	test_longline();
	test_tag_length();
	test_multiline();
	test_binary();
	test_dyn_level();
	test_args();
	test_get_tags();
	test_color();
	test_threads();
	test_va();
	test_raw_mode();
	test_throttling();
	test_change();
	test_change_threads();
	test_custom_write_func();

	return 0;
}
