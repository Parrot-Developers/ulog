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
 * libulog-obus: redirect obus logging to ulog.
 *
 */

#include <string.h>
#include "libobus.h"

#define ULOG_TAG obus
#include "ulog.h"
#include "ulog_obus.h"

ULOG_DECLARE_TAG(obus);

static void obus_func(enum obus_log_level level, const char *fmt, va_list args)
{
	/* libobus and libulog use the same level scale with an offset */
	ULOG_PRI_VA((uint32_t)(level-OBUS_LOG_CRITICAL+ULOG_CRIT), fmt, args);
}

void ulog_obus_redirect(void)
{
	/* set new handler */
	obus_log_set_cb(&obus_func);
}

void ulog_obus_set_level(int level)
{
	ULOG_SET_LEVEL(level);
}
