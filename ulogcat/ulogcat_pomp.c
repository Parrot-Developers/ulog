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

static struct pomp_ctx *pctx;
static uid_t system_uid;

void control_pomp_send_status(struct pomp_conn *conn)
{
	int enabled, alive;
	const char *target;
	const char *filename;

	if (pctx) {
		enabled = is_persist_enabled();
		alive = is_persist_alive();
		filename = get_persist_filename() ? : "";
		target = get_persist_target() ? : "";

		if (conn) {
			pomp_conn_send(conn, ULOGCAT_PERSIST_STATUS, "%d%d%s%s",
				       enabled, alive, filename, target);
		} else {
			/* if conn is NULL, send to all clients */
			pomp_ctx_send(pctx, ULOGCAT_PERSIST_STATUS, "%d%d%s%s",
				      enabled, alive, filename, target);
		}
	}
}

static void pomp_server_event(struct pomp_ctx *ctx,
			      enum pomp_event event,
			      struct pomp_conn *conn,
			      const struct pomp_msg *msg,
			      void *userdata)
{
	uint32_t id;

#if 0 /* FIXME: we need to authorize a non-privileged java app here... */
	const struct ucred *ucred;

	/* reject messages from users other than root and system */
	ucred = pomp_conn_get_peer_cred(conn);
	if ((ucred == NULL) || ((ucred->uid != system_uid) && ucred->uid)) {
		INFO("rejecting message from uid %d\n", (int)ucred->uid);
		return;
	}
#endif

	switch (event) {
	case POMP_EVENT_CONNECTED:
	case POMP_EVENT_DISCONNECTED:
		DEBUG("libpomp: %s\n", pomp_event_str(event));
		break;

	case POMP_EVENT_MSG:
		id = pomp_msg_get_id(msg);

		DEBUG("pomp: received event id %d\n", (int)id);

		switch (id) {
		case ULOGCAT_PERSIST_QUERY:
			control_pomp_send_status(conn);
			break;
		case ULOGCAT_PERSIST_ENABLE:
			enable_persist();
			break;
		case ULOGCAT_PERSIST_DISABLE:
			disable_persist();
			break;
		case ULOGCAT_PERSIST_FLUSH:
			flush_persist();
			break;
		case ULOGCAT_PERSIST_EJECT:
			eject_persist();
			break;
		default:
			break;
		}
		break;

	default:
		DEBUG("libpomp: unknown event : %d", event);
		break;
	}
}

static struct sockaddr *make_pomp_addr(struct options *op, socklen_t *len)
{
	char name[64];
	static struct sockaddr_un sun;

	memset(&sun, 0, sizeof(sun));
	snprintf(name, sizeof(name), "@/com/parrot/ulogcat/%s",
		 op->persist_pomp_addr);
	sun.sun_family = AF_LOCAL;
	strncpy(sun.sun_path, name, sizeof(sun.sun_path));
	sun.sun_path[0] = '\0';
	*len = sizeof(sun);

	return (struct sockaddr *)&sun;
}

int init_control_pomp(struct options *op, struct pollfd *fds)
{
	int ret;
	socklen_t addrlen;
	struct sockaddr *addr;
	struct passwd *pw;

	if (!op->persist_filename || !op->persist_pomp_addr)
		return 0;

	/* create a pomp server for Android Settings remote control */
	addr = make_pomp_addr(op, &addrlen);

	pctx = pomp_ctx_new(&pomp_server_event, NULL);
	if (pctx == NULL) {
		INFO("cannot create libpomp context: %s\n", strerror(errno));
		return -1;
	}

	ret = pomp_ctx_listen(pctx, addr, addrlen);
	if (ret < 0) {
		INFO("pomp_ctx_listen: %s\n", strerror(-ret));
		return -1;
	}

	/* only accept messages from user 'system' */
	pw = getpwnam("system");
	if (pw == NULL) {
		INFO("getpwnam(system): %s\n", strerror(errno));
		return -1;
	}

	system_uid = pw->pw_uid;

	fds[FD_CONTROL_POMP].fd = pomp_ctx_get_fd(pctx);
	fds[FD_CONTROL_POMP].events = POLLIN;

	return 0;
}

int process_control_pomp(struct pollfd *fds, struct ulogcat_context *ctx)
{
	int ret;

	if (fds[FD_CONTROL_POMP].revents & POLLIN) {
		/* flush ring buffers to output channel */
		ulogcat_flush_descriptors(ctx);
		/* this will call our callback function pomp_server_event() */
		ret = pomp_ctx_process_fd(pctx);
		if (ret < 0)
			INFO("pomp_ctx_process_fd: %s\n", strerror(-ret));
	}

	return 0;
}

void shutdown_control_pomp(void)
{
	if (pctx) {
		pomp_ctx_stop(pctx);
		pomp_ctx_destroy(pctx);
		pctx = NULL;
	}
}

static void pomp_client_event(struct pomp_ctx *ctx,
			      enum pomp_event event,
			      struct pomp_conn *conn,
			      const struct pomp_msg *msg,
			      void *userdata)
{
	uint32_t id;
	int enabled, alive;
	char *filename = NULL, *target = NULL;

	switch (event) {
	case POMP_EVENT_CONNECTED:
	case POMP_EVENT_DISCONNECTED:
		INFO("libpomp: %s\n", pomp_event_str(event));
		break;

	case POMP_EVENT_MSG:
		id = pomp_msg_get_id(msg);
		INFO("pomp: received event id %d\n", (int)id);

		if (id == ULOGCAT_PERSIST_STATUS) {
			/* coverity[pw.bad_printf_format_string] */
			pomp_msg_read(msg, "%d%d%ms%ms",
				      &enabled, &alive, &filename, &target);

			INFO("STATUS: enabled=%d alive=%d "
			     "filename=%s target=%s\n",
			     enabled, alive, filename, target);

			free(filename);
			free(target);
		}
		break;

	default:
		DEBUG("libpomp: unknown event : %d", event);
		break;
	}
}

void control_pomp_client(struct options *op)
{
	struct sockaddr *addr;
	socklen_t len;
	struct pollfd pfd[2];
	char buf[80];
	ssize_t rcount;
	int id, ret;
	char *p;

	if (!op->persist_pomp_addr) {
		INFO("please specify a pomp address with option -P\n");
		return;
	}
	addr = make_pomp_addr(op, &len);

	pctx = pomp_ctx_new(&pomp_client_event, NULL);
	if (pctx == NULL) {
		INFO("cannot create libpomp context: %s\n", strerror(errno));
		return;
	}

	ret = pomp_ctx_connect(pctx, addr, len);
	if (ret < 0) {
		INFO("pomp_ctx_connect : %s", strerror(-ret));
		goto finish;
	}

	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;

	pfd[1].fd = pomp_ctx_get_fd(pctx);
	pfd[1].events = POLLIN;

	INFO("Type one of the following characters and press Enter:\n"
	     "  a (abort, stop the test)\n"
	     "  f (flush)\n"
	     "  j (eject)\n"
	     "  e (enable)\n"
	     "  d (disable)\n"
	     "  q (query)\n\n");

	while (1) {
		ret = poll(pfd, 2, -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			INFO("poll: %s\n", strerror(errno));
			break;
		}

		if (pfd[0].revents & POLLIN) {

			/* parse command */
			rcount = read(pfd[0].fd, buf, sizeof(buf));
			if (rcount <= 0)
				break;

			buf[sizeof(buf)-1] = '\0';
			p = strchr(buf, '\n');
			if (p == NULL)
				continue;
			*p = '\0';

			switch (toupper(buf[0])) {
			case 'A':
				goto finish;
			case 'F':
				id = ULOGCAT_PERSIST_FLUSH;
				break;
			case 'E':
				id = ULOGCAT_PERSIST_ENABLE;
				break;
			case 'D':
				id = ULOGCAT_PERSIST_DISABLE;
				break;
			case 'Q':
				id = ULOGCAT_PERSIST_QUERY;
				break;
			case 'J':
				id = ULOGCAT_PERSIST_EJECT;
				break;
			default:
				id = -1;
				break;
			}
			if (id < 0)
				continue;

			/* send message (no payload) */
			(void)pomp_ctx_send(pctx, id, NULL);
		}

		/* pomp event */
		if (pfd[1].revents & POLLIN) {
			ret = pomp_ctx_process_fd(pctx);
			if (ret < 0)
				INFO("pomp_ctx_process_fd: %s\n",
				     strerror(-ret));
		}
	}

finish:
	shutdown_control_pomp();
}
