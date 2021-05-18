/**
 * Copyright (C) 2021 Parrot S.A.
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
 * kmsgd, a daemon copying kernel messages to a ulog buffer
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/klog.h>

/* use a new tag, log tools use kmsgd
 * as a special marker for kernel logs
 */
#define ULOG_TAG kernel_evt
#include <ulog.h>

#include "kmsgd_evt.h"

ULOG_DECLARE_TAG(kernel_evt);

void gen_evt(char *p)
{
	/* oops, panic and warn finish with something like
	 * "---[ end trace 0c0bbabe3aa774d5 ]---"
	 * (see print_oops_end_marker)
	 * catch it
	 */
	const char oops_marker[] = "---[ end trace ";
	char *oops;

	/* try to catch warn, die(oops) and panic */
	oops = strstr(p, oops_marker);
	if (oops) {
		char *end;
		oops += sizeof(oops_marker) - 1;
		end = strchr(oops, ' ');
		if (end) {
			*end = '\0';
			ULOG_EVT("KANOMALY", "type='kernel';id='%s'", oops);
		}
	}
}
