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

#if defined(ANDROID) && !defined(ANDROID_NDK)

#include <cutils/properties.h>

void set_num_property(const char *key, int num)
{
	char value[32];

	snprintf(value, sizeof(value), "%u", (unsigned int)num);
	(void)property_set(key, value);
}

int get_num_property(const char *key)
{
	char value[PROPERTY_VALUE_MAX];

	(void)property_get(key, value, "");

	return (value[0] != '\0') ? atoi(value) : -1;
}

#else

#ifdef BUILD_LIBPUTILS

/* Linux (pulsar) version */

#include <putils/properties.h>

void set_num_property(const char *key, int num)
{
	char value[32];

	snprintf(value, sizeof(value), "%u", (unsigned int)num);
	(void)sys_prop_set(key, value);
}

int get_num_property(const char *key)
{
	char value[SYS_PROP_VALUE_MAX];

	(void)sys_prop_get(key, value, "");

	return (value[0] != '\0') ? atoi(value) : -1;
}

#else

void set_num_property(const char *key, int num)
{
}

int get_num_property(const char *key)
{
	return -1;
}

#endif /* BUILD_LIBPUTILS */

#endif /* ANDROID */
