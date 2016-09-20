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
 * libulog: a minimalistic logging library derived from Android logger
 *
 */

#ifndef _PARROT_ULOG_COMMON_H
#define _PARROT_ULOG_COMMON_H

#define ULOG_EXPORT __attribute__((visibility("default")))
#ifndef __unused
#  define __unused    __attribute__((unused))
#endif

#ifdef ANDROID
#  ifdef ANDROID_NDK
#    include <android/log.h>
#  else
#    include "cutils/log.h"
#  endif
#endif /* !ANDROID */

#ifdef ANDROID
static inline int ulog_is_android(void) { return 1; }
#else /* !ANDROID */
static inline int ulog_is_android(void) { return 0; }
#endif /* !ANDROID */

void ulog_writer_android(uint32_t prio, struct ulog_cookie *cookie,
			 const char *buf, int len __unused);

#endif /* _PARROT_ULOG_COMMON_H */
