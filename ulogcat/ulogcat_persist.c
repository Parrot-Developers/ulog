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
 * A few bits are derived from Android logcat.
 *
 */

#include "ulogcat.h"

static int          persist_enabled;
static const char  *persist_filename;
static const char  *persist_target;
static char        *persist_path;
static int          persist_size;
static int          persist_logs;
static uint64_t     persist_next_try;
static FILE        *persist_fp;
static int          persist_needs_remount_ro;
static char        *persist_remount_target;
static int          persist_output_count;
static int          persist_first_write;

static int remount_target(const char *target, int rw)
{
	int ret;
	unsigned int flags = MS_REMOUNT;

	INFO("remounting '%s' read-%s\n", target, rw ? "write" : "only");

	if (!rw)
		flags |= MS_RDONLY;

	ret = mount(NULL, target, NULL, flags, NULL);
	if (ret < 0)
		INFO("mount(%s, MS_REMOUNT): %s\n", target, strerror(errno));

	return ret;
}

static void umount_target(const char *target)
{
	int ret;

	/* lazy unmount with option MNT_DETACH = 2 */
	ret = umount2(target, 2);
	if (ret)
		INFO("cannot umount2('%s', 2): %s\n", target, strerror(errno));
}

static uint64_t gettime(void)
{
	struct timespec ts = {0, 0};
	(void)clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec*1000ULL + ts.tv_nsec/1000000ULL;
}

static void close_persist(void)
{
	if (persist_fp) {
		fclose(persist_fp);
		persist_fp = NULL;
		persist_next_try = 0ULL;
		control_pomp_send_status(NULL);
	}

	if (persist_needs_remount_ro) {
		(void)remount_target(persist_remount_target, 0);
		persist_needs_remount_ro = 0;
	}
}

void flush_persist(void)
{
	int fd, ret;
	char *dirname, *p;

	if (persist_fp == NULL)
		return;

	DEBUG("persist: flushing to disk\n");

	/* from libc to kernel */
	ret = fflush(persist_fp);
	if (ret < 0)
		goto fail;

	/* from kernel to disk */
	ret = fsync(fileno(persist_fp));
	if (ret < 0)
		goto fail;

	/* also sync directory */
	dirname = strdup(persist_path);
	if (dirname) {
		p = strrchr(dirname, '/');
		if (p) {
			*p = '\0';
			fd = open(dirname, O_RDONLY);
			if (fd >= 0) {
				(void)fsync(fd);
				close(fd);
			}
		}
		free(dirname);
	}
	return;
fail:
	close_persist();
}

static void open_persist(void)
{
	int ret, rw;
	uint64_t now;
	struct stat st;

	if (!persist_filename || persist_fp || !persist_enabled)
		return;

	now = gettime();

	/* limit the rate at which we try to open output file */
	if (now < persist_next_try)
		return;

	persist_next_try = now + PERSIST_TRY_PERIOD_MS;
	persist_needs_remount_ro = 0;

	/* do we have a target ? */
	persist_target = get_target(&rw);
	if (persist_target == NULL)
		goto fail;

	if (!rw) {
		/* we need to remount this partition read-write */
		ret = remount_target(persist_target, 1);
		if (ret)
			goto fail;
		persist_needs_remount_ro = 1;
		/* save remount target */
		free(persist_remount_target);
		persist_remount_target = strdup(persist_target);
	}

	/* update path */
	free(persist_path);
	if (asprintf(&persist_path, "%s/%s", persist_target,
		     persist_filename) < 0)
		goto fail;

	persist_fp = fopen(persist_path, "a");
	if (persist_fp) {
		ret = fstat(fileno(persist_fp), &st);
		if (ret < 0) {
			INFO("persist: cannot stat '%s': %s",
			     persist_path, strerror(errno));
			goto fail;
		} else {
			persist_output_count = st.st_size;
		}
	} else {
		DEBUG("persist: fopen(%s): %s\n", persist_path,
		      strerror(errno));
	}

	control_pomp_send_status(NULL);

	return;
fail:
	close_persist();
}

static void rotate_logs(void)
{
	int i;
	char file0[512], file1[512];

	/* temporarily close, do not remount read-only */
	if (persist_fp) {
		fclose(persist_fp);
		persist_fp = NULL;
		persist_next_try = 0ULL;
	}

	for (i = persist_logs; i > 0; i--) {
		snprintf(file1, sizeof(file1), "%s.%d", persist_path, i);

		if (i == 1)
			snprintf(file0, sizeof(file0), "%s", persist_path);
		else
			snprintf(file0, sizeof(file0), "%s.%d", persist_path,
				 i-1);

		DEBUG("persist: rename '%s' -> '%s'\n", file0, file1);

		(void)rename(file0, file1);
	}

	/* now really close and force re-open */
	close_persist();
}

void persist_output_handler(unsigned char *ptr, unsigned int len)
{
	ssize_t wcount;

	open_persist();

	if (persist_fp == NULL)
		return;
	/*
	 * Add a banner on first session write, to help splitting logs
	 * into session chunks.
	 */
	if (persist_first_write && persist_output_count) {
		wcount = fprintf(persist_fp, PERSIST_DELIMITER_BANNER);
		persist_first_write = 0;
		if (wcount > 0)
			persist_output_count += wcount;
	}

	do {
		wcount = fwrite(ptr, 1, len, persist_fp);
	} while ((wcount < 0) && (errno == EINTR));

	if (wcount >= 0) {
		persist_output_count += wcount;
		if ((persist_output_count/1024) >= persist_size)
			rotate_logs();
	} else {
		DEBUG("persist: fwrite: %s\n", strerror(errno));
		close_persist();
	}
}

void enable_persist(void)
{
	int old_enabled;

	if (persist_filename) {
		old_enabled = persist_enabled;
		persist_enabled = 1;
		/* warning: persist into a _global_ property */
		set_num_property("persist.ulogcat.persist", 1);
		/* force open to detect problems early */
		refresh_persist();
		/* this may add a spurious status notification */
		if (old_enabled != persist_enabled)
			control_pomp_send_status(NULL);
	}
}

void disable_persist(void)
{
	int old_enabled;

	if (persist_filename) {
		old_enabled = persist_enabled;
		persist_enabled = 0;
		set_num_property("persist.ulogcat.persist", 0);
		flush_persist();
		close_persist();
		/* this may add a spurious status notification */
		if (old_enabled != persist_enabled)
			control_pomp_send_status(NULL);
	}
}

void eject_persist(void)
{
	const char *target;

	if (persist_filename) {
		flush_persist();
		/*
		 * Force partition unmount to allow ejection and ignore the
		 * device for later persistent storage.
		 * Notes:
		 * - jubamountd will not remount it automatically (this is what
		 *   we want)
		 * - if partition was already rw, then do nothing; unmounting it
		 *   would be too risky
		 * - persist state (enabled/disabled) remains unchanged
		 */
		if (persist_needs_remount_ro) {
			target = persist_target;
			close_persist();
			INFO("forcing unmount of %s for ejection\n", target);
			umount_target(target);
		}
	}
}

void restore_persist(void)
{
	int value;

	if (persist_filename) {
		value = get_num_property("persist.ulogcat.persist");
		if (value >= 0) {
			if (value)
				enable_persist();
			else
				disable_persist();
		}
	}
}

int is_persist_enabled(void)
{
	return persist_enabled;
}

int is_persist_alive(void)
{
	return persist_fp != NULL;
}

const char *get_persist_filename(void)
{
	return persist_filename;
}

const char *get_persist_target(void)
{
	return persist_target;
}

int init_persist(struct options *op)
{
	if (op->persist_filename) {
		persist_filename = op->persist_filename;
		persist_size     = op->persist_size;
		persist_logs     = op->persist_logs;
		persist_first_write = 1;
	}
	return 0;
}

void shutdown_persist(void)
{
	if (persist_filename) {
		flush_persist();
		close_persist();
		persist_enabled = 0;
		persist_filename = NULL;
		free(persist_path);
		free(persist_remount_target);
		persist_path = NULL;
	}
}

void refresh_persist(void)
{
	int rw;
	const char *target;

	if (persist_fp) {
		/* check if we are still mounted */
		target = get_target(&rw);
		if (target && rw && (strcmp(persist_target, target) == 0))
			/* everything looks ok */
			return;
		/* something bad happened */
		close_persist();
	} else {
		persist_next_try = 0ULL;
		open_persist();
	}
}
