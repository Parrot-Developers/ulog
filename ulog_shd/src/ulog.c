/**
 * Copyright (C) 2019 Parrot Drones SAS
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
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <string.h>
#include <futils/futils.h>
#include <libshdata.h>
#include <ulog.h>
#include <ulog_shd.h>

static struct {
	bool shd_enabled;
	struct shd_ctx *shd;
	uint16_t index;
} ctrl = {
	.shd_enabled = false,
	.index = 1,
};

#define ULOG_WRITE_RATE_USEC 10000

static void ulog_shd_write(uint32_t prio, struct ulog_cookie *cookie,
			   const char *buf, int len)
{
	struct shd_sample sample;
	struct ulog_shd_blob blob;
	char thread_name[16];
	int offset = 0;

	/* Avoid recursion of ulog messages from libshdata */
	if (!ctrl.shd_enabled)
		return;

	blob.prio = (unsigned char)(prio & ULOG_PRIO_LEVEL_MASK);

	/* Get thread name */
	if (prctl(PR_GET_NAME, (unsigned long)thread_name, 0, 0, 0) == 0) {
		/* some OS doesn't null terminate */
		thread_name[15] = '\0';
		blob.thnsize = snprintf(blob.buf, ULOG_BUF_SIZE, "%s",
					thread_name);
		if (blob.thnsize < 0)
			blob.thnsize = 0;
		else if (blob.thnsize >= ULOG_BUF_SIZE)
			/* output was truncated */
			blob.thnsize = ULOG_BUF_SIZE;
		else
			/* add the terminating null byte in buffer */
			blob.thnsize += 1;

	} else {
		blob.thnsize = 0;
	}

	blob.tid = (uint32_t)pthread_self();

	offset += blob.thnsize;
	if (offset < ULOG_BUF_SIZE) {
		blob.tagsize = MIN(cookie->namesize, ULOG_BUF_SIZE - offset);
		memcpy(&blob.buf[offset], cookie->name, blob.tagsize);
	} else {
		blob.tagsize = 0;
	}

	offset += blob.tagsize;
	if (offset < ULOG_BUF_SIZE) {
		blob.logsize = MIN(len, ULOG_BUF_SIZE - offset);
		memcpy(&blob.buf[offset], buf, blob.logsize);
	} else {
		blob.logsize = 0;
	}

	offset += blob.logsize;
	if (offset == ULOG_BUF_SIZE)
		blob.buf[ULOG_BUF_SIZE - 1] = '\0';

	time_get_monotonic(&sample.ts);
	blob.index = ctrl.index++;

	sample.cdata = (void *)&blob;
	sample.data_size = sizeof(blob);

	/* Disable logging to shd while logging to shd */
	ctrl.shd_enabled = false;

	shd_write(ctrl.shd, &sample);

	ctrl.shd_enabled = true;
}

int ulog_shd_init(const char *section_name, uint32_t max_nb_logs)
{
	struct shd_header hdr;
	unsigned int meta;
	int res;

	hdr.sample_count = max_nb_logs;
	hdr.sample_size = sizeof(struct ulog_shd_blob);
	hdr.sample_rate = ULOG_WRITE_RATE_USEC;
	hdr.metadata_size = sizeof(unsigned int);

	res = shd_create2(section_name, NULL, &hdr, &meta, &ctrl.shd);
	if (res < 0) {
		printf("failed to create section ulog in shdata: %s",
		       strerror(-res));
		return res;
	}

	ulog_set_write_func(&ulog_shd_write);
	ctrl.shd_enabled = true;

	return 0;
}

