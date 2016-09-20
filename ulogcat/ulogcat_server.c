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

static int server_fd = -1;
static int output_fd = -1;

void server_output_handler(unsigned char *buf, unsigned int size)
{
	int ret;

	if (output_fd >= 0) {
		do {
			ret = write(output_fd, buf, size);
		} while ((ret < 0) && (errno == EINTR));
	}
}

/* lifted from obus */
static int set_socket_keepalive(int fd, int keepidle, int keepintvl,
				int keepcnt)
{
	int ret;
	int keepalive = 1;

	/* activate keep alive on socket */
	ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive,
			sizeof(int));
	if (ret < 0) {
		INFO("setsockopt(SO_KEEPALIVE): %s\n", strerror(errno));
		goto out;
	}

	/*
	 * TCP_KEEPIDLE:
	 * The time (in seconds) the connection needs to remain
	 * idle before TCP starts sending keepalive probes,
	 * if the socket option SO_KEEPALIVE has been set on this socket.
	 */
	ret = setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
	if (ret < 0) {
		INFO("setsockopt(TCP_KEEPIDLE): %s\n", strerror(errno));
		goto out;
	}

	/*
	 * TCP_KEEPINTVL:
	 * The time (in seconds) between individual keepalive probes.
	 */
	ret = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
	if (ret < 0) {
		INFO("setsockopt(TCP_KEEPINTVL): %s\n", strerror(errno));
		goto out;
	}

	/*
	 * TCP_KEEPCNT:
	 * The maximum number of keepalive probes TCP
	 * should send before dropping the connection.
	 */
	ret = setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
	if (ret < 0) {
		INFO("setsockopt(TCP_KEEPCNT): %s\n", strerror(errno));
		goto out;
	}
out:
	return ret;
}

int init_server(int port, struct pollfd *fds)
{
	int ret = -1, opt = 1;
	struct sockaddr_in name;

	fds[FD_SERVER].fd = -1;
	fds[FD_CLIENT].fd = -1;

	if (port == 0)
		return 0;

	server_fd = socket(PF_INET, SOCK_STREAM|02000000, 0);
	if (server_fd < 0) {
		INFO("socket: %s\n", strerror(errno));
		goto finish;
	}

	ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
			 sizeof(int));
	if (ret < 0) {
		INFO("setsockopt: %s\n", strerror(errno));
		goto finish;
	}

	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(server_fd, (struct sockaddr *)&name, sizeof(name));
	if (ret < 0) {
		INFO("bind: %s\n", strerror(errno));
		goto finish;
	}

	ret = listen(server_fd, 1);
	if (ret < 0) {
		INFO("listen: %s\n", strerror(errno));
		goto finish;
	}

	fds[FD_SERVER].fd = server_fd;
	fds[FD_SERVER].events = POLLIN;
	fds[FD_CLIENT].fd = -1;
	fds[FD_CLIENT].events = POLLHUP|POLLRDHUP|POLLERR;

finish:
	return ret;
}

int process_server(struct pollfd *fds)
{
	int newfd;
	struct sockaddr_in name;
	socklen_t size = sizeof(name);

	if (fds[FD_CLIENT].revents & fds[FD_CLIENT].events) {
		/* client disconnected */
		close(output_fd);
		output_fd = -1;
		fds[FD_CLIENT].fd = -1;
		DEBUG("client disconnected\n");
	}

	if (fds[FD_SERVER].revents & POLLIN) {
		/* client connection */
		newfd = accept(server_fd, (struct sockaddr *)&name, &size);
		if (newfd < 0) {
			INFO("accept: %s\n", strerror(errno));
			return -1;
		}

		if (set_socket_keepalive(newfd, 5, 1, 2) < 0) {
			close(newfd);
			return -1;
		}

		DEBUG("connection from %s:%d\n", inet_ntoa(name.sin_addr),
		      (int)ntohs(name.sin_port));

		/* coverity[check_after_sink] */
		if (output_fd < 0) {
			output_fd = newfd;
			fds[FD_CLIENT].fd = newfd;
		} else {
			/* we already have a client */
			close(newfd);
		}
	}

	return 0;
}

void shutdown_server(void)
{
	if (server_fd >= 0) {
		close(server_fd);
		server_fd = -1;
	}
	if (output_fd >= 0) {
		close(output_fd);
		output_fd = -1;
	}
}

int is_client_connected(void)
{
	return output_fd >= 0;
}
