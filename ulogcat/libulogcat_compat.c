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
 * libulogcat, a reader library for ulogger/kernel log buffers
 *
 */

#include "libulogcat_private.h"

/* v1 context wrapper */
struct ulogcat_context {
	struct ulogcat3_context *ctx;        /* v3 context */
	struct ulogcat_opts_v3   opts;       /* v3 options */
	int                      opt_clear;  /* legacy option */
	const char             **devices;    /* ulog devices array */
	int                      ndevices;   /* number of ulog devices */
};

/* v1 API */

LIBULOGCAT_API struct ulogcat_context *
ulogcat_create(const struct ulogcat_opts *opts)
{
	struct ulogcat_context *v1 = NULL;

	/* old features, not supported anymore */
	if (opts->opt_binary  ||
	    opts->opt_getsize ||
	    opts->opt_rotate_filename)
		goto fail;

	v1 = calloc(1, sizeof(*v1));
	if (v1 == NULL)
		goto fail;

	v1->opts.opt_format = opts->opt_format;
	v1->opts.opt_flags = (ULOGCAT_FLAG_ULOG |
				  (opts->opt_color ? ULOGCAT_FLAG_COLOR : 0) |
				  (opts->opt_dump  ? ULOGCAT_FLAG_DUMP  : 0));
	v1->opts.opt_output_fd = opts->output_fd;
	v1->opts.opt_tail = opts->opt_tail;
	v1->opt_clear = opts->opt_clear;

	/* lazy context creation, waiting until devices are added */
	return v1;
fail:

	free(v1);
	return NULL;
}

LIBULOGCAT_API void ulogcat_destroy(struct ulogcat_context *v1)
{
	if (v1) {
		ulogcat3_close(v1->ctx);
		free(v1->devices);
		free(v1);
	}
}

LIBULOGCAT_API const char *ulogcat_strerror(struct ulogcat_context *v1)
{
	/* not supported anymore */
	return "";
}

LIBULOGCAT_API int ulogcat_add_device(struct ulogcat_context *v1,
				      const char *name)
{
	int ret = -1;

	v1->devices = realloc(v1->devices,
			      (v1->ndevices+1)*sizeof(*v1->devices));
	if (v1->devices) {
		v1->devices[v1->ndevices++] = name;
		ret = 0;
	}

	return ret;
}

static int process_logs(struct ulogcat3_context *ctx)
{
	int ret;

	while (1) {
		/* this will block until some entries are available */
		ret = ulogcat3_process_logs(ctx, 0);
		if (ret < 0) {
			break;
		} else if (ret == 0)
			/* no more processing needed (dump mode) */
			break;
	}

	return ret;
}

LIBULOGCAT_API int ulogcat_process_logs(struct ulogcat_context *v1)
{
	int ret = -1;

	if (v1->ctx == NULL)
		v1->ctx = ulogcat3_open(&v1->opts, v1->devices, v1->ndevices);

	if (v1->ctx) {
		if (v1->opt_clear)
			ret = ulogcat3_clear(v1->ctx);
		else
			ret = process_logs(v1->ctx);
	}

	return ret;
}
