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
 * ulogcat, a reader for ulogger/logger/klog messages.
 *
 * A few bits are derived from Android logcat.
 *
 */

#include "ulogcat.h"

static void show_error(struct ulogcat_context *ctx)
{
	INFO("libulogcat: %s\n", ulogcat_strerror(ctx));
}

static void output_handler(void *data, unsigned char *ptr, unsigned int len)
{
	struct options *op = data;

	if (op->port)
		server_output_handler(ptr, len);
	else
		IGNORE_RESULT(write(STDOUT_FILENO, ptr, len));

	if (op->persist_filename)
		persist_output_handler(ptr, len);
}

static int is_output_ready(struct options *op)
{
	/* persisting data takes precedence over client socket */
	if (is_persist_alive())
		return 1;

	/* in blocking mode we wait until persist is alive */
	if (op->persist_blocking_mode)
		return 0;

	if (op->port)
		/* wait until a client is connected */
		return is_client_connected();

	/* stdout always ready */
	return 1;
}

int main(int argc, char **argv)
{
	int i, ret = -1, timeout_ms, nfds, n;
	struct options op;
	struct ulogcat_context *ctx;
	struct pollfd *fds = NULL;

	get_options(argc, argv, &op);

	if (op.persist_test_client) {
		control_pomp_client(&op);
		return 0;
	}

	op.opts.opt_output_handler = output_handler;
	op.opts.opt_output_handler_data = &op;

	ctx = ulogcat_create2(&op.opts);
	if (ctx == NULL) {
		INFO("cannot allocate ulogcat context\n");
		goto finish;
	}

	/* add explicitly requested ulog buffers */
	for (i = 0; i < op.ulog_ndevices; i++) {
		ret = ulogcat_add_device(ctx, op.ulog_devices[i]);
		if (ret) {
			show_error(ctx);
			goto finish;
		}
	}

	/* add explicitly requested Android buffers */
	for (i = 0; i < op.alog_ndevices; i++) {
		ret = ulogcat_add_android_device(ctx, op.alog_devices[i]);
		if (ret) {
			show_error(ctx);
			goto finish;
		}
	}

	/* get simple actions (clear, get size) out of the way */
	if (op.opts.opt_flags & (ULOGCAT_FLAG_GET_SIZE|ULOGCAT_FLAG_CLEAR)) {
		ret = ulogcat_process_logs(ctx);
		if (ret)
			show_error(ctx);
		goto finish;
	}

	/* complete setup */
	ret = ulogcat_init(ctx);
	if (ret) {
		show_error(ctx);
		goto finish;
	}

	nfds = FD_NB + ulogcat_get_descriptor_nb(ctx);

	fds = malloc(nfds*sizeof(*fds));
	if (fds == NULL)
		goto finish;

	for (i = 0; i < nfds; i++) {
		fds[i].fd = -1;
		fds[i].events = 0;
	}

	ret = ulogcat_setup_descriptors(ctx, &fds[FD_NB], nfds-FD_NB);
	if (ret) {
		show_error(ctx);
		goto finish;
	}

	if (init_server(op.port, fds)                    ||
	    init_control(fds)                            ||
	    init_control_pomp(&op, fds)                  ||
	    init_mount_monitor(fds, op.persist_filename) ||
	    init_persist(&op))
		goto finish;

	if (op.persist_restore)
		restore_persist();
	else
		enable_persist();

	timeout_ms = (op.opts.opt_flags & ULOGCAT_FLAG_DUMP) ? 0 : -1;

	while (1) {

		n = nfds;
		/*
		 * Rudimentary flow control: do not poll libulogcat
		 * descriptors when we are not ready to output messages.
		 */
		if (!is_output_ready(&op)) {
			n = FD_NB;
			timeout_ms = -1;
		}

		ret = poll(fds, n, timeout_ms);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			INFO("poll: %s", strerror(errno));
			break;
		}

		if (process_server(fds)        ||
		    process_mount_monitor(fds) ||
		    process_control(fds, ctx)  ||
		    process_control_pomp(fds, ctx))
			break;

		if (n == FD_NB)
			continue;

		ret = ulogcat_process_descriptors(ctx, ret, &timeout_ms);
		if (ret == 1) {
			ret = 0;
			break;
		} else if (ret < 0) {
			break;
		}

		/* limit CPU and I/O usage in persist (non-interactive) mode */
		if (is_persist_enabled()) {
			timeout_ms = 1000;
			sleep(1);
		}
	}

finish:
	shutdown_persist();
	shutdown_mount_monitor();
	shutdown_control_pomp();
	shutdown_control();
	shutdown_server();

	ulogcat_destroy(ctx);
	free(op.ulog_devices);
	free(op.alog_devices);
	free(fds);

	return ret;
}
