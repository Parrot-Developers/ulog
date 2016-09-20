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

 * @file ulog_write_android
 *
 * @brief forward ulog to android log system
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ulog.h"
#include "ulogger.h"
#include "ulog_common.h"

void ulog_writer_android(uint32_t prio, struct ulog_cookie *cookie,
			 const char *buf, int len __unused)
{
#ifdef ANDROID
	static const int prio_map[] = {
		[ULOG_CRIT] = ANDROID_LOG_FATAL,
		[ULOG_ERR] = ANDROID_LOG_ERROR,
		[ULOG_WARN] = ANDROID_LOG_WARN,
		[ULOG_NOTICE] = ANDROID_LOG_INFO,
		[ULOG_INFO] = ANDROID_LOG_INFO,
		[ULOG_DEBUG] = ANDROID_LOG_DEBUG,
	};

	if (prio & (1U << ULOG_PRIO_BINARY_SHIFT))
		/* skip binary data */
		return;

	__android_log_write(prio_map[prio & ULOG_PRIO_LEVEL_MASK],
			    cookie->name, buf);
#endif /* !ANDROID */
}

