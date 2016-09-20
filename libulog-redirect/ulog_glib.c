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
 * libulog-glib: redirect glib logging to ulog.
 *
 */

#include <string.h>
#include <glib.h>
#include "ulog.h"
#include "ulog_glib.h"

/* master cookie, will not be actually used for messages */
static ULOG_DECLARE_TAG(ulog_glib);

/**
 */
static void glog_func(const gchar *domain, GLogLevelFlags level,
		const gchar *message, gpointer userdata)
{
	int masterlevel, uloglevel = -1;
	struct ulog_cookie cookie;

	/* make sure domain is valid */
	if (domain == NULL)
		domain = "APP";

	/* convert levels (ERROR and CRITICAL levels have different
	   meaning for glib) */
	if ((level&G_LOG_LEVEL_ERROR) != 0)
		uloglevel = ULOG_CRIT;
	else if ((level&G_LOG_LEVEL_CRITICAL) != 0)
		uloglevel = ULOG_ERR;
	else if ((level&G_LOG_LEVEL_WARNING) != 0)
		uloglevel = ULOG_WARN;
	else if ((level&G_LOG_LEVEL_MESSAGE) != 0)
		uloglevel = ULOG_INFO;
	else if ((level&G_LOG_LEVEL_INFO) != 0)
		uloglevel = ULOG_INFO;
	else if ((level&G_LOG_LEVEL_DEBUG) != 0)
		uloglevel = ULOG_DEBUG;

	/* master cookie level, this should have been initialized */
	masterlevel = (__ULOG_REF(ulog_glib)).level;

	if ((uloglevel != -1) && (masterlevel >= 0)) {
		/* use temporary cookie */
		cookie.name = domain;
		cookie.namesize = strlen(domain)+1;
		/* this is safe only because level is non-negative */
		cookie.level = masterlevel;
		cookie.next = NULL;
		/* do the print */
		ulog_log_str(uloglevel, &cookie, message);
	}
}

/**
 */
void ulog_glib_redirect(void)
{
	/* make sure cookie is registered now */
	ULOG_INIT(ulog_glib);
	/* set new handler */
	g_log_set_default_handler(&glog_func, NULL);
}

void ulog_glib_set_level(int level)
{
	return ulog_set_level(&__ULOG_REF(ulog_glib), level);
}
