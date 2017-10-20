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
 * @file ulogctl.h
 *
 * @brief ulogctl API.
 * ulogctl_srv : server to set ulog tag level
 * according to the ulogctl_cli requests.
 * ulogctl_cli : client to send requests to ulogctl_srv to set ulog tag level.
 */

#ifndef _ULOGCTL_H_
#define _ULOGCTL_H_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>

#include <libpomp.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** To be used for all public API */
#ifdef ULOGCTL_API_EXPORTS
#  define ULOGCTL_API	__attribute__((visibility("default")))
#else /* !ULOGCTL_API_EXPORTS */
#  define ULOGCTL_API
#endif /* !ULOGCTL_API_EXPORTS */

/* Internal forward declarations */
/* Server to set ulog tag level according to the ulogctl_cli requests. */
struct ulogctl_srv;
/* Client to send requests to ulogctl_srv to set ulog tag level. */
struct ulogctl_cli;

/**
 * Create a ulog controller server.
 * @param addr : socket address of the ulogctl server.
 * @param addrlen: socket address length.
 * @param loop : pomp loop.
 * @param ret_ctr : will receive the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_srv_new(struct sockaddr *addr,
		size_t addrlen,
		struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr);

/**
 * Create a ulog controller server on inet socket.
 * @param port : port to use.
 * @param loop : pomp loop.
 * @param ret_ctr : will receive the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_srv_new_inet(int port,
		struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr);

/**
 * Create a ulog controller server on unix socket.
 *
 * @warning Only available on linux platform.
 *
 * @param sock : socket name.
 * @param loop : pomp loop.
 * @param ret_ctr : will receive the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @note if <sock> start by '@' character, an abstract socket
 * with the same name will be used.
 */
int ulogctl_srv_new_unix(char *sock, struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr);

/**
 * Create a ulog controller server on unix abstract socket
 * named with the current process name.
 * @param loop : pomp loop.
 * @param ret_ctr : will receive the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_srv_new_unix_proc(struct pomp_loop *loop,
		struct ulogctl_srv **ret_ctr);

/**
 * Destroy ulog controller server.
 * @param self : the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_srv_destroy(struct ulogctl_srv *self);

/**
 * Start ulog controller server.
 * @param self : the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_srv_start(struct ulogctl_srv *self);

/**
 * Stop ulog controller server.
 * @param self : the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_srv_stop(struct ulogctl_srv *self);

enum ulogctl_req_status {
	REQUEST_DONE = 0, /*< Request correctly done. */
	REQUEST_ERROR, /*< Request error. */
};

/**
 * ulogctl client request callbacks.
 */
struct ulogctl_cli_cbs {
	/** User data given in callbacks */
	void *userdata;

	/**
	 * Notify request status.
	 * @param status : request status.
	 * @param userdata :  user data.
	 */
	void (*request_status) (enum ulogctl_req_status status,
			void *userdata);

	/**
	 * Notify tag info.
	 * @param tag : tag name.
	 * @param level : log level of the tag. See ULOG priority levels.
	 * @param userdata :  user data.
	 */
	void (*tag_info) (const char *tag, int level, void *userdata);
};

/**
 * Create a ulog controller client.
 * @param addr : address of the ulogctl server.
 * @param addrlen: address length.
 * @param loop : pomp loop.
 * @param cbs : request callbacks.
 * @param ret_ctr : will receive the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_new(struct sockaddr *addr,
		size_t addrlen,
		struct pomp_loop *loop,
		struct ulogctl_cli_cbs *cbs,
		struct ulogctl_cli **ret_ctr);

/**
 * Create a ulog controller client.
 * @param proc_name : process name of the ulog controller server to connect.
 * @param loop : pomp loop.
 * @param cbs : request callbacks.
 * @param ret_ctr : will receive the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_new_proc(const char *proc_name,
		struct pomp_loop *loop,
		struct ulogctl_cli_cbs *cbs,
		struct ulogctl_cli **ret_ctr);

/**
 * Destroy ulog controller client.
 * @param self : the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_destroy(struct ulogctl_cli *self);

/**
 * Start ulog controller client.
 * @param self : the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_start(struct ulogctl_cli *self);

/**
 * Stop ulog controller client.
 * @param self : the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_stop(struct ulogctl_cli *self);

/**
 * Set the log level of a tag.
 * @param self : the controller object.
 * @param tag : tag name.
 * @param level : log level to set. See ULOG priority levels.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_set_tag_level(struct ulogctl_cli *self,
		const char *tag, int level);

/**
 * Set the log level for all tags.
 * @param self : the controller object.
 * @param level : log level to set. See ULOG priority levels.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_set_all_level(struct ulogctl_cli *self,  int level);

/**
 * List all tags.
 * @param self : the controller object.
 * @return 0 in case of success, negative errno value in case of error.
 */
int ulogctl_cli_list(struct ulogctl_cli *self);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_ULOGCTL_H_ */
