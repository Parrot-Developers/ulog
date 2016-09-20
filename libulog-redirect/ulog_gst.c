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
 * libulog-gst: redirect gst logging to ulog.
 *
 */

#define _GNU_SOURCE
#include <string.h>
#include <gst/gst.h>
#include "ulog.h"
#include "ulog_gst.h"

/* Should be enough to contain GStreamer debug messages */
#define GST_MAX_ENTRY_LEN 2048

/* master cookie, will not be actually used for messages */
static ULOG_DECLARE_TAG(ulog_gst);

/**
 */

/**
 * Prettify object printing by adding a header to the message depending
 * of the object type.
 *
 * @param object Object to be printed
 * @param str    Write output to this buffer
 * @param size   Output buffer size in bytes
 *
 * @return The number of bytes written to this buffer or a negative value
 * if an error has been encountered.
 */
static int prettify_object(GObject *object, char *str, size_t size)
{
	int ret = 0;

	if (object == NULL) {
		ret = snprintf(str, size, "(NULL)");
	} else if (GST_IS_PAD(object) && GST_OBJECT_NAME(object)) {
		ret = snprintf(str, size, "<%s:%s>",
				GST_DEBUG_PAD_NAME(object));
	} else if (GST_IS_OBJECT(object) && GST_OBJECT_NAME(object)) {
		ret = snprintf(str, size, "<%s>", GST_OBJECT_NAME(object));
	} else if (G_IS_OBJECT(object)) {
		ret = snprintf(str, size, "<%s@%p>", G_OBJECT_TYPE_NAME(object),
				object);
	} else {
		/* Should not happen but... */
		ret = snprintf(str, size, "%p", object);
	}

	if (ret < 0)
		goto done;

	if ((size_t)ret >= size) {
		/* output was truncated */
		ret = size - 1;
	}

done:
	return ret;
}

static void gst_log_func(GstDebugCategory *category, GstDebugLevel level,
		const gchar *file, const gchar *function, gint line,
		GObject *object, GstDebugMessage *message, gpointer userdata)
{
	int masterlevel, uloglevel = -1;
	struct ulog_cookie cookie;
	const gchar *catname = NULL;
	char buf[GST_MAX_ENTRY_LEN];
	const size_t size = sizeof(buf);
	int offset = 0;

	/* make sure the message should be displayed */
	if (level > gst_debug_category_get_threshold(category))
		return;

	/* make sure category is valid */
	catname = gst_debug_category_get_name(category);
	if (catname == NULL)
		catname = "APP";

	/* convert levels */
	if (level == GST_LEVEL_ERROR)
		uloglevel = ULOG_ERR;
	else if (level == GST_LEVEL_WARNING)
		uloglevel = ULOG_WARN;
	else if (level == GST_LEVEL_FIXME)
		uloglevel = ULOG_WARN;
	else if (level == GST_LEVEL_INFO)
		uloglevel = ULOG_INFO;
	else if (level == GST_LEVEL_DEBUG)
		uloglevel = ULOG_DEBUG;
	else if (level == GST_LEVEL_LOG)
		uloglevel = ULOG_DEBUG;
	else if (level >= GST_LEVEL_TRACE)
		uloglevel = ULOG_DEBUG;

	/* master cookie level, this should have been initialized */
	masterlevel = (__ULOG_REF(ulog_gst)).level;

	if ((uloglevel != -1) && (masterlevel >= 0)) {
		const char *filename;
		int ret;

		/* use temporary cookie */
		cookie.name = catname;
		cookie.namesize = strlen(catname)+1;
		/* this is safe only because level is non-negative */
		cookie.level = masterlevel;
		cookie.next = NULL;

		/* print message location, ie file:line:function
		 * for file, only keeps basename to make message lighter */
		filename = basename(file);
		if (*filename == '\0')
			filename = "unknown";

		ret = snprintf(buf + offset, size - offset, "%s:%d:%s",
				filename, line, function);
		if (ret > 0)
			offset += ret;

		/* make object display look nicer */
		ret = prettify_object(object, buf + offset, size - offset);
		if (ret > 0)
			offset += ret;

		if (snprintf(buf + offset, size - offset, ":%s",
				gst_debug_message_get(message)) < 0)
			return;

		ulog_log_str(uloglevel, &cookie, buf);
	}
}

/**
 */
void ulog_gst_redirect(void)
{
	/* make sure cookie is registered now */
	ULOG_INIT(ulog_gst);
	/* set new handler */
	gst_debug_remove_log_function(&gst_debug_log_default);
	gst_debug_add_log_function(&gst_log_func, NULL, NULL);
	gst_debug_set_default_threshold(GST_LEVEL_FIXME);
}

void ulog_gst_set_level(int level)
{
	return ulog_set_level(&__ULOG_REF(ulog_gst), level);
}
