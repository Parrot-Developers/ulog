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
 * @file ulogctl_cli.c
 *
 * @brief ulogctl client.
 * Client to send requests to ulogctl_srv to set ulog tag level.
 */

#include <libpomp.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <stdio.h>
#include <stddef.h>

#include <ulogctl.h>
#include "ulogctl_priv.h"

/** */
enum ulogctl_cli_state {
	ULOGCTR_CLI_STATE_IDLE,
	ULOGCTR_CLI_STATE_CONNECTING,
	ULOGCTR_CLI_STATE_CONNECTED,
};

/** tag struct (used for sorting) */
struct tag {
	int level;
	const char *name;
};

/** ulog controller client */
struct ulogctl_cli {
	/* running loop */
	struct pomp_ctx         *pomp_ctx;
	/* message to send */
	struct pomp_msg         *msg;
	/* address */
	struct sockaddr_storage addr;
	/* address length*/
	size_t                  addrlen;
	/* connection status */
	enum ulogctl_cli_state  state;
	/* callback */
	struct ulogctl_cli_cbs  cbs;
	/* tags list (used for sorting) */
	size_t tags_count;
	struct tag *tags;
};

/* Decode tag info message. */
static void decode_tag_info_msg(struct ulogctl_cli *self,
		const struct pomp_msg *msg)
{
	int res = 0;
	char *tag = NULL;
	int level = 0;
	struct tag *tags;

	RETURN_IF_FAILED(msg != NULL, -EINVAL);

	res = pomp_msg_read(msg, ULOGCTL_MSG_FMT_DEC_TAG_INFO,
			&tag,
			&level);

	/* save tag in order to send the whole list sorted
	 * all at once after the LIST_END message */
	self->tags_count++;
	tags = realloc(self->tags, self->tags_count * sizeof(*self->tags));
	if (!tags) {
		self->tags_count--;
		return;
	}
	self->tags = tags;
	self->tags[self->tags_count-1].level = level;
	self->tags[self->tags_count-1].name = tag;
}

static int sort_increasing_name(const void *a, const void *b)
{
	const struct tag *tag_a = a;
	const struct tag *tag_b = b;

	/* sort by increasing tag name order */
	return strcmp(tag_a->name, tag_b->name);
}

/* Decode end of list message. */
static void decode_list_end_msg(struct ulogctl_cli *self,
		const struct pomp_msg *msg)
{
	size_t i;
	struct tag *tag;

	self->cbs.request_status(REQUEST_DONE, self->cbs.userdata);
	pomp_msg_destroy(self->msg);
	self->msg = NULL;

	/* sort the tag list by increasing name order */
	qsort(self->tags, self->tags_count, sizeof(*self->tags),
	      &sort_increasing_name);

	/* send the whole sorted tag list all at once */
	for (i = 0; i < self->tags_count; i++) {
		tag = &self->tags[i];
		self->cbs.tag_info(tag->name, tag->level, self->cbs.userdata);
	}
}

/* Process the messages received. */
static void process_msg(const struct pomp_msg *msg, struct ulogctl_cli *self)
{
	RETURN_IF_FAILED(msg != NULL, -EINVAL);
	RETURN_IF_FAILED(self != NULL, -EINVAL);

	switch (pomp_msg_get_id(msg)) {
	case ULOGCTL_MSG_ID_TAG_INFO:
		decode_tag_info_msg(self, msg);
		break;
	case ULOGCTL_MSG_ID_TAG_LIST_END:
		decode_list_end_msg(self, msg);
		break;
	default:
		ULOGE("Message id unknown (%d)", pomp_msg_get_id(msg));
		break;
	}
}

static int send_msg(struct ulogctl_cli *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);
	RETURN_ERR_IF_FAILED(self->msg != NULL, -EINVAL);

	res = pomp_ctx_send_msg(self->pomp_ctx, self->msg);
	if (res < 0) {
		LOG_ERRNO("pomp_ctx_send_msg", -res);
		return res;
	}

	pomp_msg_destroy(self->msg);
	self->msg = NULL;
	return 0;
}

static void connected(struct ulogctl_cli *self)
{
	RETURN_IF_FAILED(self != NULL, -EINVAL);

	if (self->state == ULOGCTR_CLI_STATE_CONNECTING) {
		self->state = ULOGCTR_CLI_STATE_CONNECTED;
		if (self->msg != NULL) {
			/* Send the message in waiting. */
			send_msg(self);
		}
	} else {
		ULOGW("Unexpected state (%d)", self->state);
	}
}

/* Pomp event callback. */
static void event_cb(struct pomp_ctx *ctx,
		enum pomp_event event,
		struct pomp_conn *conn,
		const struct pomp_msg *msg,
		void *userdata)
{
	struct ulogctl_cli *self = userdata;
	int res = 0;

	RETURN_IF_FAILED(self != NULL, -EINVAL);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		ULOGD("POMP_EVENT_CONNECTED ...");
		connected(self);

		break;
	case POMP_EVENT_DISCONNECTED:
		ULOGD("POMP_EVENT_DISCONNECTED ...");
		res = ulogctl_cli_stop(self);
		if (res < 0)
			LOG_ERRNO("ulogctl_cli_stop", -res);

		break;
	case POMP_EVENT_MSG:
		process_msg(msg, self);
		break;
	}
}

/* Callback of pomp message sent. */
static void send_cb(struct pomp_ctx *ctx,
		struct pomp_conn *conn,
		struct pomp_buffer *buf,
		uint32_t status,
		void *cookie,
		void *userdata)
{
	struct ulogctl_cli *self = userdata;

	RETURN_IF_FAILED(self != NULL, -EINVAL);
	RETURN_IF_FAILED(self->msg != NULL, -EINVAL);

	switch (pomp_msg_get_id(self->msg)) {
	case ULOGCTL_MSG_ID_SET_TAG_LEV:
		self->cbs.request_status(REQUEST_DONE, self->cbs.userdata);
		pomp_msg_destroy(self->msg);
		self->msg = NULL;
		break;
	case ULOGCTL_MSG_ID_SET_ALL_LEV:
		self->cbs.request_status(REQUEST_DONE, self->cbs.userdata);
		pomp_msg_destroy(self->msg);
		self->msg = NULL;
		break;
	case ULOGCTL_MSG_ID_LIST_TAGS:
		/* do nothing */
		break;
	default:
		ULOGE("Message id unknown (%d)", pomp_msg_get_id(self->msg));
		self->cbs.request_status(REQUEST_ERROR, self->cbs.userdata);
		pomp_msg_destroy(self->msg);
		self->msg = NULL;
		break;
	}
}

ULOGCTL_API int ulogctl_cli_new_proc(const char *proc_name,
		struct pomp_loop *loop,
		struct ulogctl_cli_cbs *cbs,
		struct ulogctl_cli **ret_ctr)
{
	struct sockaddr_un addr;
	size_t addrlen;

	RETURN_ERR_IF_FAILED(proc_name != NULL, -EINVAL);

	/* Form an AF_UNIX socket address  */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	addrlen = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s%s",
				PROCESS_SOCK_PREFIX,
				proc_name);
	addrlen += offsetof(struct sockaddr_un, sun_path);
	/* Form an abstract socket address  */
	addr.sun_path[0] = '\0';

	return ulogctl_cli_new((struct sockaddr *)&addr, addrlen, loop, cbs,
			ret_ctr);
}

ULOGCTL_API int ulogctl_cli_new(struct sockaddr *addr,
		size_t addrlen,
		struct pomp_loop *loop,
		struct ulogctl_cli_cbs *cbs,
		struct ulogctl_cli **ret_ctr)
{
	struct ulogctl_cli *ulogctl = NULL;
	int res = 0;

	RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	RETURN_ERR_IF_FAILED(ret_ctr != NULL, -EINVAL);
	RETURN_ERR_IF_FAILED(cbs != NULL, -EINVAL);
	RETURN_ERR_IF_FAILED(cbs->request_status != NULL, -EINVAL);
	RETURN_ERR_IF_FAILED(cbs->tag_info != NULL, -EINVAL);
	*ret_ctr = NULL;

	/* Allocate structure */
	ulogctl = calloc(1, sizeof(*ulogctl));
	if (ulogctl == NULL)
		return -ENOMEM;

	/* set callback */
	ulogctl->cbs = *cbs;

	/* create pomp context */
	ulogctl->pomp_ctx = pomp_ctx_new_with_loop(&event_cb, ulogctl, loop);
	if (ulogctl->pomp_ctx == NULL) {
		res = -ENOMEM;
		goto error;
	}

	res = pomp_ctx_set_send_cb(ulogctl->pomp_ctx, send_cb);
	if (res < 0) {
		LOG_ERRNO("pomp_ctx_set_send_cb", -res);
		goto error;
	}

	/* copy the address */
	memcpy(&ulogctl->addr, addr, addrlen);
	ulogctl->addrlen = addrlen;

	*ret_ctr = ulogctl;
	return 0;

error:

	ulogctl_cli_destroy(ulogctl);

	return res;
}

ULOGCTL_API int ulogctl_cli_destroy(struct ulogctl_cli *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	res = pomp_ctx_destroy(self->pomp_ctx);
	if (res < 0)
		LOG_ERRNO("pomp_ctx_destroy", -res);

	free(self);
	return 0;
}

ULOGCTL_API int ulogctl_cli_start(struct ulogctl_cli *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	if (self->state != ULOGCTR_CLI_STATE_IDLE)
		return -EBUSY;

	self->state = ULOGCTR_CLI_STATE_CONNECTING;

	res = pomp_ctx_connect(self->pomp_ctx, (struct sockaddr *)&self->addr,
			self->addrlen);
	if (res < 0) {
		LOG_ERRNO("pomp_ctx_listen", -res);
		return res;
	}

	return 0;
}

ULOGCTL_API int ulogctl_cli_stop(struct ulogctl_cli *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	if (self->state == ULOGCTR_CLI_STATE_IDLE)
		return 0;

	self->state = ULOGCTR_CLI_STATE_IDLE;

	if (self->msg != NULL) {
		self->cbs.request_status(REQUEST_ERROR, self->cbs.userdata);
		pomp_msg_destroy(self->msg);
		self->msg = NULL;
	}

	res = pomp_ctx_stop(self->pomp_ctx);
	if (res < 0)
		LOG_ERRNO("pomp_ctx_stop", -res);

	return 0;
}

ULOGCTL_API int ulogctl_cli_set_tag_level(struct ulogctl_cli *self,
		const char *tag, int level)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);
	RETURN_ERR_IF_FAILED(tag != NULL, -EINVAL);

	if (self->state == ULOGCTR_CLI_STATE_IDLE) {
		/* Client must be started. */
		return -EPERM;
	}

	if (self->msg != NULL) {
		/* Another message is already waiting. */
		return -EBUSY;
	}

	/* Create message */
	self->msg = pomp_msg_new();
	if (self->msg == NULL) {
		res = -ENOMEM;
		goto error;
	}

	/* Encode message */
	res = pomp_msg_write(self->msg, ULOGCTL_MSG_ID_SET_TAG_LEV,
			ULOGCTL_MSG_FMT_ENC_SET_TAG_LEV,
			tag,
			level);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_write", -res);
		goto error;
	}

	if (self->state == ULOGCTR_CLI_STATE_CONNECTED) {
		/* Send it */
		res = pomp_ctx_send_msg(self->pomp_ctx, self->msg);
		if (res < 0) {
			LOG_ERRNO("pomp_ctx_send_msg", -res);
			goto error;
		}
	}
	/*else: msg will be send at the connection */

	/* successful */
	return 0;

	/* Cleanup */
error:
	if (self->msg != NULL) {
		pomp_msg_destroy(self->msg);
		self->msg = NULL;
	}
	return res;
}

ULOGCTL_API int ulogctl_cli_set_all_level(struct ulogctl_cli *self,  int level)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	if (self->state == ULOGCTR_CLI_STATE_IDLE) {
		/* Client must be started. */
		return -EPERM;
	}

	if (self->msg != NULL) {
		/* Another message is already waiting. */
		return -EBUSY;
	}

	/* Create message */
	self->msg = pomp_msg_new();
	if (self->msg == NULL) {
		res = -ENOMEM;
		goto error;
	}

	/* Encode message */
	res = pomp_msg_write(self->msg, ULOGCTL_MSG_ID_SET_ALL_LEV,
			ULOGCTL_MSG_FMT_ENC_SET_ALL_LEV,
			level);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_write", -res);
		goto error;
	}

	if (self->state == ULOGCTR_CLI_STATE_CONNECTED) {
		/* Send it */
		res = pomp_ctx_send_msg(self->pomp_ctx, self->msg);
		if (res < 0) {
			LOG_ERRNO("pomp_ctx_send_msg", -res);
			goto error;
		}
	}
	/*else: msg will be send at the connection */

	/* successful */
	return 0;

	/* Cleanup */
error:
	if (self->msg != NULL) {
		pomp_msg_destroy(self->msg);
		self->msg = NULL;
	}
	return res;
}

ULOGCTL_API int ulogctl_cli_list(struct ulogctl_cli *self)
{
	int res = 0;

	RETURN_ERR_IF_FAILED(self != NULL, -EINVAL);

	if (self->state == ULOGCTR_CLI_STATE_IDLE) {
		/* Client must be started. */
		return -EPERM;
	}

	if (self->msg != NULL) {
		/* Another message is already waiting. */
		return -EBUSY;
	}

	/* Create message */
	self->msg = pomp_msg_new();
	if (self->msg == NULL) {
		res = -ENOMEM;
		goto error;
	}

	/* clear the previous tags list if any (used for sorting) */
	free(self->tags);
	self->tags = NULL;
	self->tags_count = 0;

	/* Encode message */
	res = pomp_msg_write(self->msg, ULOGCTL_MSG_ID_LIST_TAGS,
			ULOGCTL_MSG_FMT_ENC_LIST_TAGS);
	if (res < 0) {
		LOG_ERRNO("pomp_msg_write", -res);
		goto error;
	}

	if (self->state == ULOGCTR_CLI_STATE_CONNECTED) {
		/* Send it */
		res = pomp_ctx_send_msg(self->pomp_ctx, self->msg);
		if (res < 0) {
			LOG_ERRNO("pomp_ctx_send_msg", -res);
			goto error;
		}
	}
	/*else: msg will be sent at the connection */

	/* successful */
	return 0;

	/* Cleanup */
error:
	if (self->msg != NULL) {
		pomp_msg_destroy(self->msg);
		self->msg = NULL;
	}
	return res;
}
