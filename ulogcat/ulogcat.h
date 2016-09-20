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
 */

#ifndef _ULOGCAT_H
#define _ULOGCAT_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pwd.h>

#include <libpomp.h>
#include <libulogcat.h>

#define INFO(...)        fprintf(stderr, "ulogcat: " __VA_ARGS__)
#define DEBUG(...)       do {} while (0)
/*#define DEBUG(...)       fprintf(stderr, "ulogcat: " __VA_ARGS__)*/

/* When persistent writes fail, limit open retry rate */
#define PERSIST_TRY_PERIOD_MS  (5000)

#define POMP_SOCKET_NAME       "@/com/parrot/ulogcat"

#define PERSIST_DELIMITER_BANNER \
	"====================== ULOGCAT SESSION START  ======================\n"

/* libpomp message ids with corresponding format strings */
enum {
	/* from client; format NULL */
	ULOGCAT_PERSIST_QUERY,

	/* from client; format NULL */
	ULOGCAT_PERSIST_ENABLE,

	/* from client; format NULL */
	ULOGCAT_PERSIST_DISABLE,

	/* from client; format NULL */
	ULOGCAT_PERSIST_FLUSH,

	/* from server; format "%d%d%s%s" enabled,alive,path,target */
	ULOGCAT_PERSIST_STATUS,

	/* from client; format NULL */
	ULOGCAT_PERSIST_EJECT,
};

/* fixed file descriptors */
enum {
	FD_SERVER,
	FD_CLIENT,
	FD_MOUNT_MONITOR,
	FD_CONTROL,
	FD_CONTROL_POMP,
	FD_NB,
};

struct options {
	struct ulogcat_opts_v2  opts;
	char                  **ulog_devices;
	int                     ulog_ndevices;
	char                  **alog_devices;
	int                     alog_ndevices;
	int                     port;
	const char             *persist_filename;
	int                     persist_size;
	int                     persist_logs;
	int                     persist_blocking_mode;
	int                     persist_restore;
	const char             *persist_pomp_addr;
	int                     persist_test_client;
};

void get_options(int argc, char **argv, struct options *op);

int init_server(int port, struct pollfd *fds);
int init_control(struct pollfd *fds);
int init_control_pomp(struct options *op, struct pollfd *fds);
int init_mount_monitor(struct pollfd *fds, const char *path);
int init_persist(struct options *op);

int process_server(struct pollfd *fds);
int process_control(struct pollfd *fds, struct ulogcat_context *ctx);
int process_control_pomp(struct pollfd *fds, struct ulogcat_context *ctx);
int process_mount_monitor(struct pollfd *fds);

void shutdown_server(void);
void shutdown_control(void);
void shutdown_control_pomp(void);
void shutdown_mount_monitor(void);
void shutdown_persist(void);

void server_output_handler(unsigned char *buf, unsigned int size);
void persist_output_handler(unsigned char *buf, unsigned int size);

const char *get_target(int *rw);

void enable_persist(void);
void disable_persist(void);
int is_persist_enabled(void);
int is_persist_alive(void);
void flush_persist(void);
void refresh_persist(void);
void restore_persist(void);
void eject_persist(void);
const char *get_persist_filename(void);
const char *get_persist_target(void);

int is_client_connected(void);

void control_pomp_send_status(struct pomp_conn *conn);
void control_pomp_client(struct options *op);

void set_num_property(const char *name, int num);
int get_num_property(const char *name);

/*
 * -Wunused-result is impossible to silence with a cast, see
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
 * This ugly hack is (c) YMM
 */
#define IGNORE_RESULT(_e) ((void)((_e) || 0))

#endif /* _ULOGCAT_H */
