/**
 * Copyright (C) 2016 Parrot S.A.
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
 * @file ulogctl_svr.c
 *
 * @brief ulogctl server.
 * Server to set ulog tag level according to the ulogctl_cli requests.
 */

#include <libpomp.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __linux__
#  include <sys/prctl.h>
#endif

#include <ulogctl.h>
#include "ulogctl_priv.h"

/* Maximum length of process name :  max process length (16) + '\0' */
#define PROCESS_NAME_MAX_LEN 17

/** ulog controller server */
struct ulogctl_srv {
	/* pomp context */
	struct pomp_ctx         *pomp_ctx;
	/* address */
	struct sockaddr_storage addr;
	/* address length */
	size_t                  addrlen;
	/* state */
	int                     started;
};

/* Decode set log level for a tag message. */
static void decode_set_tag_level_msg(const struct pomp_msg *msg)
{
	int res = 0;
	char *tag = NULL;
	int level = 0;

	RETURN_IF_FAILED(msg != NULL, -EINVAL);

	res = pomp_msg_read(msg, ULOGCTL_MSG_FMT_DEC_SET_TAG_LEV,
			&tag,
			&level);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_read", -res);
		return;
	}

	res = ulog_set_tag_level(tag, level);
	if (res < 0) {
		ULOGE("Failed to set the tag \"%s\" to the level (%d) "
				"err=%d(%s)", tag, level, -res, strerror(-res));
	}
}

/* set the log level for a cookie. */
static void set_level_cb(struct ulog_cookie *cookie,
		void *userdata)
{
	int *level = userdata;

	RETURN_IF_FAILED(level != NULL, -EINVAL);

	ulog_set_level(cookie, *level);
}


/* Decode set all log level message. */
static void decode_set_all_level_msg(const struct pomp_msg *msg)
{
	int res = 0;
	int level = 0;

	RETURN_IF_FAILED(msg != NULL, -EINVAL);

	res = pomp_msg_read(msg, ULOGCTL_MSG_FMT_DEC_SET_ALL_LEV,
			&level);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_read", -res);
		return;
	}

	res = ulog_foreach(set_level_cb, &level);
	if (res < 0)
		LOG_ERRNO("ulog_foreach", -res);
}

/* Send tag info message. */
static void send_tag_info_cb(struct ulog_cookie *cookie, void *userdata)
{
	struct pomp_conn *conn = userdata;
	int res = 0;
	struct pomp_msg *msg = NULL;

	RETURN_IF_FAILED(conn != NULL, -EINVAL);

	/* Create message */
	msg = pomp_msg_new();
	if (msg == NULL) {
		res = -ENOMEM;
		goto error;
	}

	/* Encode message */
	res = pomp_msg_write(msg, ULOGCTL_MSG_ID_TAG_INFO,
			ULOGCTL_MSG_FMT_ENC_TAG_INFO,
			cookie->name,
			cookie->level);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_write", -res);
		goto error;
	}

	/* Send it */
	res = pomp_conn_send_msg(conn, msg);
	if (res < 0) {
		LOG_ERRNO("pomp_ctx_send_msg", -res);
		goto error;
	}

	/* Cleanup */
error:
	if (msg != NULL) {
		pomp_msg_destroy(msg);
		msg = NULL;
	}
}

/* Send end of tag list message. */
static int send_list_end_msg(struct ulogctl_srv *self)
{
	int res = 0;
	struct pomp_msg *msg = NULL;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	/* Create message */
	msg = pomp_msg_new();
	if (msg == NULL) {
		res = -ENOMEM;
		goto error;
	}

	/* Encode message */
	res = pomp_msg_write(msg, ULOGCTL_MSG_ID_TAG_LIST_END,
			ULOGCTL_MSG_FMT_ENC_TAG_LIST_END);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_write", -res);
		goto error;
	}

	/* Send it */
	res = pomp_ctx_send_msg(self->pomp_ctx, msg);
	if (res < 0) {
		LOG_ERRNO("pomp_ctx_send_msg", -res);
		goto error;
	}

	/* successful */
	if (msg != NULL) {
		pomp_msg_destroy(msg);
		msg = NULL;
	}
	return 0;

	/* Cleanup */
error:
	if (msg != NULL) {
		pomp_msg_destroy(msg);
		msg = NULL;
	}
	return res;
}

/* Decode ask of tag list message. */
static void decode_list_msg(struct pomp_conn *conn,
		const struct pomp_msg *msg,
		struct ulogctl_srv *ulogctl)
{
	int res = 0;

	RETURN_IF_FAILED(msg != NULL, -EINVAL);

	res = pomp_msg_read(msg, ULOGCTL_MSG_FMT_DEC_LIST_TAGS);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_read", -res);
		return;
	}

	res = ulog_foreach(send_tag_info_cb, conn);
	if (res < 0) {
		LOG_ERRNO("ulog_foreach", -res);
		return;
	}

	res = send_list_end_msg(ulogctl);
	if (res < 0)
		LOG_ERRNO("send_list_end_msg", -res);
}

/* Process the messages received. */
static void process_msg(struct pomp_conn *conn,
		const struct pomp_msg *msg, struct ulogctl_srv *ulogctl)
{
	RETURN_IF_FAILED(msg != NULL, -EINVAL);
	RETURN_IF_FAILED(ulogctl != NULL, -EINVAL);

	switch (pomp_msg_get_id(msg)) {
	case ULOGCTL_MSG_ID_SET_TAG_LEV:
		decode_set_tag_level_msg(msg);
		break;
	case ULOGCTL_MSG_ID_SET_ALL_LEV:
		decode_set_all_level_msg(msg);
		break;
	case ULOGCTL_MSG_ID_LIST_TAGS:
		decode_list_msg(conn, msg, ulogctl);
		break;
	default:
		ULOGE("Message id unknown (%d)", pomp_msg_get_id(msg));
		break;
	}
}

/* Pomp event callback. */
static void event_cb(struct pomp_ctx *ctx,
		enum pomp_event event,
		struct pomp_conn *conn,
		const struct pomp_msg *msg,
		void *userdata)
{
	struct ulogctl_srv *self = userdata;

	RETURN_IF_FAILED(self != NULL, -EINVAL);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		ULOGD("POMP_EVENT_CONNECTED ...");
		break;
	case POMP_EVENT_DISCONNECTED:
		ULOGD("POMP_EVENT_DISCONNECTED ...");
		break;
	case POMP_EVENT_MSG:
		process_msg(conn, msg, self);
		break;
	}
}

ULOGCTL_API int ulogctl_srv_new(struct sockaddr *addr,
		size_t addrlen,
		struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr)
{
	struct ulogctl_srv *ulogctl = NULL;

	int res = 0;

	RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	RETURN_ERR_IF_FAILED(ret_ctr != NULL, -EINVAL);
	*ret_ctr = NULL;

	/* Allocate structure */
	ulogctl = calloc(1, sizeof(*ulogctl));
	if (ulogctl == NULL)
		return -ENOMEM;

	ulogctl->pomp_ctx = pomp_ctx_new_with_loop(&event_cb, ulogctl, loop);
	if (res < 0) {
		ULOGE("pomp_ctx_new_with_loop failed.");
		goto error;
	}

	/* copy the address */
	memcpy(&ulogctl->addr, addr, addrlen);
	ulogctl->addrlen = addrlen;

	*ret_ctr = ulogctl;
	return 0;

error:
	ulogctl_srv_destroy(ulogctl);
	return res;
}

ULOGCTL_API int ulogctl_srv_new_inet(int port, struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr)
{
	struct sockaddr_in addr;

	/* Setup address to list on given port */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	return ulogctl_srv_new((struct sockaddr *) &addr, sizeof(addr), loop,
			ret_ctr);
}

ULOGCTL_API int ulogctl_srv_new_unix(char *sock, struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr)
{
	struct sockaddr_un addr;
	size_t addrlen;

	/*
	 * Form an AF_UNIX socket address:
	 */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy((addr.sun_path), sock, sizeof(addr.sun_path)-1);
	addr.sun_path[sizeof(addr.sun_path)-1] = '\0';
	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(sock);
	if (addr.sun_path[0] == '@')
		addr.sun_path[0] = '\0';

	return ulogctl_srv_new((struct sockaddr *) &addr, addrlen, loop,
			ret_ctr);
}

ULOGCTL_API int ulogctl_srv_new_unix_proc(struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr)
{
#ifdef __linux__
	int res = 0;
	char proc_name[PROCESS_NAME_MAX_LEN];
	char sock_name[PROCESS_SOCK_MAX_LEN];

	/* Get process name. */
	res = prctl(PR_GET_NAME, proc_name, NULL, NULL, NULL);
	if (res < 0) {
		LOG_ERRNO("prctl", errno);
		return -errno;
	}
	proc_name[PROCESS_NAME_MAX_LEN - 1] = '\0';

	snprintf(sock_name, sizeof(sock_name), "%s%s",
			PROCESS_SOCK_PREFIX,
			proc_name);

	return ulogctl_srv_new_unix(sock_name, loop, ret_ctr);
#else /* !__linux__ */
	return -ENOSYS;
#endif /* !__linux__ */
}

ULOGCTL_API int ulogctl_srv_destroy(struct ulogctl_srv *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	res = pomp_ctx_destroy(self->pomp_ctx);
	if (res < 0)
		LOG_ERRNO("pomp_ctx_destroy", -res);

	free(self);
	return 0;
}

ULOGCTL_API int ulogctl_srv_start(struct ulogctl_srv *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	if (self->started)
		return -EBUSY;

	res = pomp_ctx_listen(self->pomp_ctx, (struct sockaddr *)&self->addr,
			self->addrlen);
	if (res < 0) {
		LOG_ERRNO("pomp_ctx_listen", -res);
		return res;
	}

	self->started = 1;
	return 0;
}

ULOGCTL_API int ulogctl_srv_stop(struct ulogctl_srv *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	if (!self->started)
		return 0;

	res = pomp_ctx_stop(self->pomp_ctx);
	if (res < 0)
		LOG_ERRNO("pomp_ctx_stop", -res);

	self->started = 0;
	return res;
}
