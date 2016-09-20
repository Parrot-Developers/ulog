/**
 * Copyright (C) 2014 Parrot S.A.
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
 * ulogcat, a reader for ulogger/logger/klog messages.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/klog.h>

#include <libulogcat.h>

#define ULOG_TAG libulogcat_test
#include <ulog.h>
ULOG_DECLARE_TAG(libulogcat_test);

#define INFO(...)        fprintf(stderr, "ulogcat-test: " __VA_ARGS__)

#define TRACE(...)							\
	do {								\
		fprintf(stderr, "%s: ", __func__);			\
		fprintf(stderr, __VA_ARGS__);				\
		fprintf(stderr, "\n");					\
	} while (0)

#define LOG_MASK (ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_ALOG|ULOGCAT_FLAG_KLOG)

#define TMP_FILENAME "/tmp/libulogcat-test"

#define KMSGD_WAIT_US 10000

static int has_android;

static void ulog(const char *buf)
{
	ULOG_STR(ULOG_INFO, buf);
}

static void klog(const char *buf)
{
	int fd;

	fd = open("/dev/kmsg", O_WRONLY);
	if (fd >= 0) {
		write(fd, buf, strlen(buf));
		close(fd);
	}
}

static int can_use_alog(void)
{
	int fd, ret = 0;

	fd = open("/dev/log/main", O_WRONLY);
	if ((fd < 0) && (errno == ENOENT))
		fd = open("/dev/log_main", O_WRONLY);

	if (fd >= 0) {
		ret = 1;
		close(fd);
	}
	return ret;
}

static void alog(const char *buf)
{
	int fd, ret;
	struct iovec vec[3];
	const unsigned char prio = 4;
	const char tag[] = "libulogcat_test";

	fd = open("/dev/log/main", O_WRONLY);
	if ((fd < 0) && (errno == ENOENT))
		fd = open("/dev/log_main", O_WRONLY);

	if (fd >= 0) {
		vec[0].iov_base = (void *)&prio;
		vec[0].iov_len = 1;
		vec[1].iov_base = (void *)tag;
		vec[1].iov_len = sizeof(tag);
		vec[2].iov_base = (void *)buf;
		vec[2].iov_len = strlen(buf)+1;

		do {
			ret = writev(fd, vec, 3);
		} while ((ret < 0) && (errno == EINTR));

		close(fd);
	}
}

static void sendlog(unsigned int flags, const char *fmt, ...)
{
	va_list ap;
	char buf[128];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (flags & ULOGCAT_FLAG_ULOG)
		ulog(buf);
	if (flags & ULOGCAT_FLAG_ALOG)
		alog(buf);
	if (flags & ULOGCAT_FLAG_KLOG)
		klog(buf);
}

static void run(struct ulogcat_opts_v2 *opts)
{
	int ret;
	struct ulogcat_context *ctx;

	ctx = ulogcat_create2(opts);
	assert(ctx);

	ret = ulogcat_process_logs(ctx);
	if (ret) {
		INFO("libulogcat: %s\n", ulogcat_strerror(ctx));
		assert(!(ret));
	}

	ulogcat_destroy(ctx);
}

static void clear(unsigned int flags)
{
	struct ulogcat_opts_v2 opts;

	TRACE("flags = 0x%x", flags);

	memset(&opts, 0, sizeof(opts));
	opts.opt_output_fd = -1;
	opts.opt_flags = ULOGCAT_FLAG_CLEAR|flags;
	run(&opts);
}

static void show_size(unsigned int flags)
{
	struct ulogcat_opts_v2 opts;

	TRACE("flags = 0x%x", flags);

	memset(&opts, 0, sizeof(opts));
	opts.opt_output_fd = -1;
	opts.opt_flags = ULOGCAT_FLAG_GET_SIZE|flags;
	run(&opts);
}

static void test_ioctl(void)
{
	show_size(ULOGCAT_FLAG_ULOG);
	show_size(ULOGCAT_FLAG_KLOG);
	if (has_android)
		show_size(ULOGCAT_FLAG_ALOG);
	show_size(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_ALOG);
	show_size(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_KLOG);
	show_size(ULOGCAT_FLAG_KLOG|ULOGCAT_FLAG_ALOG);
	show_size(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_ALOG|ULOGCAT_FLAG_KLOG);
	clear(ULOGCAT_FLAG_ULOG);
	clear(ULOGCAT_FLAG_KLOG);
	if (has_android)
		clear(ULOGCAT_FLAG_ALOG);
	clear(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_ALOG);
	clear(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_KLOG);
	clear(ULOGCAT_FLAG_KLOG|ULOGCAT_FLAG_ALOG);
	clear(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_ALOG|ULOGCAT_FLAG_KLOG);
}

static int open_tmp_file(void)
{
	int fd;
	fd = open(TMP_FILENAME, O_WRONLY|O_CREAT|O_APPEND, 0644);
	assert(fd);
	return fd;
}

static int grep_tmp_file(const char *pattern, int binary)
{
	FILE *fp;
	char *buf;
	static char line[4096];
	int i, ret, size, match = 0;

	fp = fopen(TMP_FILENAME, "r+");
	assert(fp);

	if (binary) {
		/* replace null chars with newlines in tmp file */
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		assert(size > 0);
		fseek(fp, 0, SEEK_SET);
		buf = malloc(size);
		assert(buf);
		ret = fread(buf, 1, size, fp);
		assert(ret == size);

		for (i = 0; i < size; i++)
			if (buf[i] == '\0')
				buf[i] = '\n';
		fseek(fp, 0, SEEK_SET);
		fwrite(buf, 1, size, fp);
		free(buf);
		fseek(fp, 0, SEEK_SET);
	}

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, pattern))
			match++;
	}

	fclose(fp);

	return match;
}

static void clean_tmp_file(void)
{
	(void)remove(TMP_FILENAME);
}

static unsigned int weight(unsigned int data)
{
	unsigned int hweight = 0;
	static const uint8_t htab[16] = {
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
	};

	while (data) {
		hweight += htab[data & 0xf];
		data >>= 4;
	}
	return hweight;
}

static void run_format(unsigned int flags, unsigned int fmt, int binary)
{
	int matches, expected_matches;
	static int count;
	struct ulogcat_opts_v2 opts;
	char tag[64];

	TRACE("flags = 0x%x, fmt = %u binary=%d", flags, fmt, binary);

	clear(flags & LOG_MASK);

	memset(&opts, 0, sizeof(opts));
	opts.opt_output_fd = open_tmp_file();
	opts.opt_flags = ULOGCAT_FLAG_DUMP|flags;
	opts.opt_format = fmt;

	/* make a unique tag for matching lines */
	snprintf(tag, sizeof(tag), "libulogcat-test-%u-%d", fmt, count++);

	sendlog(flags & LOG_MASK, "Hello from %s, tag=%s\n", __func__, tag);

	/* leave some time for kmsgd to copy message */
	if (flags & ULOGCAT_FLAG_KLOG)
		usleep(KMSGD_WAIT_US);

	run(&opts);

	close(opts.opt_output_fd);

	/* count number of tags appearing in output */
	matches = grep_tmp_file(tag, binary);
	expected_matches = weight(flags & LOG_MASK);
	assert(matches == expected_matches);

	clean_tmp_file();
}

static void test_all_formats(unsigned int flags)
{
	run_format(flags, ULOGCAT_FORMAT_SHORT, 0);
	run_format(flags, ULOGCAT_FORMAT_ALIGNED, 0);
	run_format(flags, ULOGCAT_FORMAT_PROCESS, 0);
	run_format(flags, ULOGCAT_FORMAT_LONG, 0);
	run_format(flags, ULOGCAT_FORMAT_CSV, 0);
	run_format(flags, ULOGCAT_FORMAT_BINARY, 1);
	run_format(flags, ULOGCAT_FORMAT_CKCM, 1);
}

static void test_format(void)
{
	unsigned int flags;

	test_all_formats(ULOGCAT_FLAG_ULOG);
	test_all_formats(ULOGCAT_FLAG_KLOG);
	if (has_android)
		test_all_formats(ULOGCAT_FLAG_ALOG);

	flags = ULOGCAT_FLAG_KLOG|ULOGCAT_FLAG_ULOG;
	test_all_formats(flags);
	if (has_android)
		flags |= ULOGCAT_FLAG_ALOG;
	test_all_formats(flags);
}

#define MAGIC ((void *)0x12345678)

static void output_handler(void *data, unsigned char *buf, unsigned len)
{
	int fd;
	unsigned char pattern = (uint8_t)(uint32_t)data;

	if (pattern)
		assert((buf[0] == pattern) || (buf[0] == '-'));

	fd = open_tmp_file();
	(void)write(fd, buf, len);
	close(fd);
}

static void run_handler(unsigned int flags, char pattern)
{
	static int count;
	int matches, expected_matches;
	struct ulogcat_opts_v2 opts;
	char tag[64];

	TRACE("flags = 0x%x pattern=%c", flags, pattern);

	clear(flags & LOG_MASK);

	memset(&opts, 0, sizeof(opts));
	opts.opt_output_fd = -1;
	opts.opt_output_handler = &output_handler;
	opts.opt_output_handler_data = (void *)(uint32_t)pattern;
	opts.opt_flags = ULOGCAT_FLAG_DUMP|flags;
	opts.opt_format = ULOGCAT_FORMAT_ALIGNED;

	/* make a unique tag for matching lines */
	snprintf(tag, sizeof(tag), "libulogcat-test-%d", count++);

	sendlog(flags & LOG_MASK, "Hello from %s, tag=%s\n", __func__, tag);

	/* leave some time for kmsgd to copy message */
	if (flags & ULOGCAT_FLAG_KLOG)
		usleep(KMSGD_WAIT_US);

	run(&opts);

	/* count number of tags appearing in output */
	matches = grep_tmp_file(tag, 0);
	expected_matches = weight(flags & LOG_MASK);
	assert(matches == expected_matches);

	clean_tmp_file();
}

static void test_handler(void)
{
	run_handler(ULOGCAT_FLAG_ULOG, 0);
	run_handler(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_KLOG, 0);
}

static void test_label(void)
{
	run_handler(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_SHOW_LABEL, 'U');
	run_handler(ULOGCAT_FLAG_KLOG|ULOGCAT_FLAG_SHOW_LABEL, 'K');
	if (has_android)
		run_handler(ULOGCAT_FLAG_ALOG|ULOGCAT_FLAG_SHOW_LABEL, 'A');
}

static void test_color(void)
{
	run_format(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_KLOG|ULOGCAT_FLAG_COLOR,
		   ULOGCAT_FORMAT_LONG, 0);
}

static void run_lines(unsigned int flags, int lines)
{
	static int count;
	int matches, expected_matches, i;
	struct ulogcat_opts_v2 opts;
	char tag[64];

	TRACE("flags = 0x%x lines=%d", flags, lines);

	clear(flags & LOG_MASK);

	memset(&opts, 0, sizeof(opts));
	opts.opt_output_fd = open_tmp_file();
	opts.opt_flags = ULOGCAT_FLAG_DUMP|flags;
	opts.opt_format = ULOGCAT_FORMAT_LONG;

	/* make a unique tag for matching lines */
	snprintf(tag, sizeof(tag), "libulogcat-test-%d", count++);

	for (i = 0; i < lines; i++)
		sendlog(flags & LOG_MASK, "Hello from %s, tag=%s\n", __func__,
			tag);

	/* leave some time for kmsgd to copy message */
	if (flags & ULOGCAT_FLAG_KLOG)
		usleep(KMSGD_WAIT_US);

	run(&opts);

	close(opts.opt_output_fd);

	/* count number of tags appearing in output */
	matches = grep_tmp_file(tag, 0);
	expected_matches = weight(flags & LOG_MASK)*lines;
	assert(matches == expected_matches);

	clean_tmp_file();
}

static void test_lines(void)
{
	run_lines(ULOGCAT_FLAG_ULOG, 1000);
	run_lines(ULOGCAT_FLAG_ULOG|ULOGCAT_FLAG_KLOG, 1000);
}

int main(int argc, char *argv[])
{
	has_android = can_use_alog();

	INFO("STARTING TESTS...\n");
	test_ioctl();
	test_format();
	test_handler();
	test_label();
	test_color();
	test_lines();
	INFO("SUCCESS !\n");

	return 0;
}
