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

static int mount_fd = -1;
static char *path;
static const char *target;
static int mounted_rw;

static int scan_mount_info(const char *path, const char **target, int *rw)
{
	FILE *fp;
	int ret = -1;
	static char buf[512];
	static char filename[512];
	struct stat st;
	char *point, *token, *options, *fs;

	fp = fopen("/proc/self/mountinfo", "r");
	if (fp == NULL)
		return ret;

	while (fgets(buf, sizeof(buf), fp)) {

		(void)strtok(buf, " ");     /* mount ID */
		(void)strtok(NULL, " ");    /* parent ID */
		(void)strtok(NULL, " ");    /* major:minor */
		(void)strtok(NULL, " ");    /* root */

		point = strtok(NULL, " ");  /* mount point */
		if (!point)
			continue;

		/* check if path exists in this partition */
		snprintf(filename, sizeof(filename), "%s/%s", point, path);
		if ((stat(filename, &st) < 0) || !S_ISDIR(st.st_mode))
			continue;

		/* skip optional fields */
		do {
			token = strtok(NULL, " ");
		} while (token && strcmp(token, "-"));

		if (!token)
			continue;

		fs = strtok(NULL, " ");   /* filesystem */
		if (!fs)
			continue;

		/* whitelist filter on supported filesystems */
		if (strcmp(fs, "vfat") &&
		    strncmp(fs, "ext", 3) &&
		    strcmp(fs, "ubifs"))
			continue;

		(void)strtok(NULL, " ");   /* mount source */

		options = strtok(NULL, " "); /* per-superblock options */
		if (!options)
			continue;

		/* OK we have a match */
		ret = 0;
		*target = point;

		/* look for rw option */
		*rw = 0;
		token = strtok(options, ",");

		while (token) {
			if (strcmp(token, "rw") == 0) {
				*rw = 1;
				break;
			}
			token = strtok(NULL, ",");
		}
		break;
	}
	fclose(fp);

	return ret;
}

const char *get_target(int *rw)
{
	*rw = mounted_rw;
	return target;
}

int init_mount_monitor(struct pollfd *fds, const char *userpath)
{
	char *p;

	fds[FD_MOUNT_MONITOR].fd = -1;

	if (userpath == NULL)
		return 0;

	path = strdup(userpath);
	if (path == NULL)
		return -1;

	p = strrchr(path, '/');
	if ((p == NULL) || (p == path)) {
		INFO("persist path lacks directory part: %s\n", path);
		return -1;
	}
	*p = '\0';

	if (path[0] == '/') {
		/* if path is absolute, do not monitor mounts */
		target = "";
		mounted_rw = 1;
		fds[FD_MOUNT_MONITOR].events = 0;
		return 0;
	}

	mount_fd = open("/proc/mounts", O_RDONLY);
	if (mount_fd < 0)
		return -1;

	fds[FD_MOUNT_MONITOR].fd = mount_fd;
	fds[FD_MOUNT_MONITOR].events = POLLPRI|POLLERR;

	target = NULL;
	mounted_rw = 0;

	/* look for a matching mounted device */
	(void)scan_mount_info(path, &target, &mounted_rw);

	return 0;
}

int process_mount_monitor(struct pollfd *fds)
{
	int ret = -1;

	if (!(fds[FD_MOUNT_MONITOR].revents & fds[FD_MOUNT_MONITOR].events))
		return 0;

	target = NULL;
	mounted_rw = 0;

	/* look for a matching mounted device */
	ret = scan_mount_info(path, &target, &mounted_rw);
	if (ret == 0)
		DEBUG("found match target %s rw %d\n", target, mounted_rw);

	refresh_persist();
	return 0;
}

void shutdown_mount_monitor(void)
{
	if (mount_fd >= 0) {
		close(mount_fd);
		mount_fd = -1;
	}
	free(path);
	path = NULL;
}
