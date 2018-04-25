/**
 * Copyright (C) 2018 Parrot S.A.
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
 * libulogcat, a reader library for logger/ulogger/kernel log buffers
 *
 */

#include "libulogcat_private.h"

/*
 * As a shortcut struct ulogcat3_context is the same as struct ulogcat_context.
 * It is only forward declared, but not defined.
 */

LIBULOGCAT_API struct ulogcat3_context *
ulogcat3_open(const struct ulogcat_opts_v3 *opts_v3,
	      const char **ulog_devices, int len)
{
	struct ulogcat_context *ctx;
	struct ulogcat_opts_v2 opts_v2;
	int i, ret;

	if (!opts_v3)
		return NULL;

	if (opts_v3->opt_output_fp)
		return NULL;

	memset(&opts_v2, 0, sizeof(opts_v2));

	/* emulate v2 format */
	opts_v2.opt_format = opts_v3->opt_format;
	opts_v2.opt_flags = opts_v3->opt_flags;
	opts_v2.opt_output_fd = opts_v3->opt_output_fd;

	ctx = ulogcat_create2(&opts_v2);
	if (!ctx)
		goto fail;

	if (len && ulog_devices) {
		for (i = 0; i < len; i++) {
			ret = ulogcat_add_device(ctx, ulog_devices[i]);
			if (ret < 0)
				goto fail;
		}
	}

	ret = ulogcat_init(ctx);
	if (ret < 0)
		goto fail;

	return (struct ulogcat3_context *) ctx;

fail:
	if (ctx)
		ulogcat_destroy(ctx);

	return NULL;
}

LIBULOGCAT_API void ulogcat3_close(struct ulogcat3_context *ctx)
{
	return ulogcat_destroy((struct ulogcat_context *) ctx);
}

LIBULOGCAT_API int ulogcat3_clear(struct ulogcat3_context *ctx)
{
	int ret;
	struct ulogcat_context *c = (struct ulogcat_context *) ctx;

	c->flags |= ULOGCAT_FLAG_CLEAR;
	ret = ulogcat_do_ioctl(c);
	c->flags ^= ULOGCAT_FLAG_CLEAR;

	return ret;
}

LIBULOGCAT_API int ulogcat3_process_logs(struct ulogcat3_context *ctx,
					 int max_entries)
{
	return ulogcat_read_log_lines((struct ulogcat_context *) ctx);
}

