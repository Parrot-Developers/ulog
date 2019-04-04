/**
 * Copyright (C) 2019 Parrot Drones SAS
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
 */

#ifndef __ULOG_SHD_H__
#define __ULOG_SHD_H__

#include <ulog.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ULOG_SHD_NB_SAMPLES 2048

/* make sure the structure size is multiple of int size */
struct ulog_shd_blob {
	uint16_t index;			/* ulog message index */
	uint8_t prio;			/* Priority level */
	uint32_t tid;			/* thread id */
	int32_t thnsize;		/* Thread name size */
	int32_t tagsize;		/* tag name size */
	int32_t logsize;		/* Log message size */
	char buf[ULOG_BUF_SIZE];	/* buffer for thread/tag/log */
} __attribute__((packed, aligned(4)));

int ulog_shd_init(const char *section_name, uint32_t max_nb_logs);

#ifdef __cplusplus
}
#endif

#endif
