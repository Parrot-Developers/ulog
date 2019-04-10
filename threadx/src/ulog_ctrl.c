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
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <futils/futils.h>
#include <libshdata.h>
#include <ulog_shd.h>
#include "AmbaDataType.h"
#include "AmbaKAL.h"
#include "AmbaPrint.h"
#include "tx_thread.h"

#define ULOG_TAG ulog_ctrl
#include "ulog.h"
ULOG_DECLARE_TAG(ULOG_TAG);

static struct {
	bool shd_enabled;
	struct shd_ctx *shd;
} ctrl = {
	.shd_enabled = false,
};

#define ULOG_WRITE_RATE_USEC 10000

static void ulog_shd_put(unsigned long long ts, int prio,
			 const char *tag, int tagsize,
			 const char *log, int logsize)
{
	struct shd_sample_metadata sample_meta;
	struct ulog_shd_blob blob;
	static unsigned short int index = 1;
	TX_THREAD *current_thread;
	int offset = 0;

	if (!ctrl.shd_enabled)
		return;

	/* if no priority given consider RED color as error level
	 * otherwise default*/
	if (prio != 0)
		blob.prio = (unsigned char)(prio & ULOG_PRIO_LEVEL_MASK);
	else if (logsize >= 6 && log[0] == '\033' && log[5] == '1')
		blob.prio = ULOG_ERR;
	else
		blob.prio = ULOG_INFO;

	/* Get thread name */
	/* TODO Get the thread name in ambalog to display it on console ? */
	current_thread = tx_thread_identify();
	if (current_thread) {
		blob.thnsize = snprintf(blob.buf, ULOG_BUF_SIZE, "%s",
					current_thread->tx_thread_name);
		if (blob.thnsize < 0)
			blob.thnsize = 0;
		else if (blob.thnsize >= ULOG_BUF_SIZE)
			/* output was truncated */
			blob.thnsize = ULOG_BUF_SIZE;
		else
			/* add the terminating null byte */
			blob.thnsize += 1;

		/* We have no way to get a relevant thread id for now;
		 * let's set it to 1 as 0 identify unknown thread name. */
		blob.tid = 1;
	} else {
		blob.thnsize = 0;
		blob.tid = 0;
	}

	offset += blob.thnsize;
	if (offset < ULOG_BUF_SIZE) {
		blob.tagsize = MIN(tagsize, ULOG_BUF_SIZE - offset);
		memcpy(&blob.buf[offset], tag, blob.tagsize);
	} else {
		blob.tagsize = 0;
	}

	offset += blob.tagsize;
	if (offset < ULOG_BUF_SIZE) {
		blob.logsize = MIN(logsize,  ULOG_BUF_SIZE - offset);
		memcpy(&blob.buf[offset], log, blob.logsize);
	} else {
		blob.logsize = 0;
	}

	offset += blob.logsize;
	if (offset == ULOG_BUF_SIZE)
		blob.buf[ULOG_BUF_SIZE - 1] = '\0';

	ts *= 1000;
	time_ns_to_timespec(&ts, &sample_meta.ts);
	blob.index = index++;

	/* Disable logging to shd while logging to shd */
	ctrl.shd_enabled = false;

	shd_write_new_blob(ctrl.shd, &blob, sizeof(blob), &sample_meta);

	ctrl.shd_enabled = true;
}


void ulog_amba_shd_init(void)
{
	struct shd_hdr_user_info header;
	unsigned int meta = 0;

	header.blob_size = sizeof(struct ulog_shd_blob);
	header.max_nb_samples = ULOG_SHD_NB_SAMPLES;
	header.rate = ULOG_WRITE_RATE_USEC;
	header.blob_metadata_hdr_size = sizeof(unsigned int);

	ctrl.shd = shd_create("ulog", NULL, &header, &meta);
	if (ctrl.shd == NULL) {
		AmbaPrintColor(RED, "failed to create section ulog in shdata");
		return;
	}

	AmbaPrint_SetAlternateOutputFunc(ulog_shd_put);
	ctrl.shd_enabled = true;

	/* Shared memory is available and able to receive log messages;
	 * ulog messages don't need to be logged to the console anymore. */
	AmbaPrint_SetUlogToConsole(0);

	return;
}
