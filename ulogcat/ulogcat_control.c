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
 * ulogcat, a reader for ulogger/logger/klog messages.
 *
 * A few bits are derived from Android logcat.
 *
 */

#include "ulogcat.h"

static int control_fd[2] = {-1, -1};

static void signal_handler(int sig)
{
	char msg;

	switch (sig) {
		/* just flush */
	case SIGALRM:
		msg = 'F';
		break;
	case SIGUSR1:
		/* enable persistent logging */
		msg = 'E';
		break;
	case SIGUSR2:
		/* disable persistent logging */
		msg = 'D';
		break;
		/* flush and quit on other signals */
	default:
		msg = 'Q';
		break;
	}

	if (control_fd[1] >= 0)
		IGNORE_RESULT(write(control_fd[1], &msg, sizeof(msg)));
}

int init_control(struct pollfd *fds)
{
	struct sigaction act;

	fds[FD_CONTROL].fd = -1;

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);
	sigaction(SIGUSR2, &act, NULL);
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGPIPE, &act, NULL);

	/* create a signalling mechanism for control messages */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, control_fd) < 0) {
		INFO("socketpair: %s\n", strerror(errno));
		return -1;
	}

	(void)fcntl(control_fd[0], F_SETFD, FD_CLOEXEC);
	(void)fcntl(control_fd[0], F_SETFL, O_NONBLOCK);
	(void)fcntl(control_fd[1], F_SETFD, FD_CLOEXEC);
	(void)fcntl(control_fd[1], F_SETFL, O_NONBLOCK);

	fds[FD_CONTROL].fd = control_fd[0];
	fds[FD_CONTROL].events = POLLIN;

	return 0;
}

int process_control(struct pollfd *fds, struct ulogcat_context *ctx)
{
	int ret = 0;
	char command;

	if (!(fds[FD_CONTROL].revents & POLLIN))
		return 0;

	if (read(control_fd[0], &command, sizeof(command)) == 1) {

		DEBUG("received command '%c'\n", command);

		/* flush ring buffers to output channel */
		ulogcat_flush_descriptors(ctx);

		switch (command) {
		case 'Q':
			/* force termination */
			ret = 1;
			/* fall-through */
		case 'F':
			flush_persist();
			break;
		case 'E':
			enable_persist();
			break;
		case 'D':
			disable_persist();
			break;
		case 'J':
			eject_persist();
			break;
		default:
			/* ignore other commands */
			break;
		}
	}

	return ret;
}

void shutdown_control(void)
{
	if (control_fd[0] >= 0) {
		close(control_fd[0]);
		control_fd[0] = -1;
	}
	if (control_fd[1] >= 0) {
		close(control_fd[1]);
		control_fd[1] = -1;
	}
}
