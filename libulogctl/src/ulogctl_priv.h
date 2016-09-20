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
 * @file ulogctl_priv.h
 */

#ifndef _ULOGCTL_PRIV_H_
#define _ULOGCTL_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ULOG_TAG ulogctl
#include "ulog.h"

/** Log error with errno */
#define LOG_ERR(_func, _err) \
	ULOGE("%s err=%d(%s)", _func, _err, strerror(_err))

/** Log error with fd and errno */
#define LOG_FD_ERR(_func, _fd, _err) \
	ULOGE("%s(fd=%d) err=%d(%s)", _func, _fd, _err, strerror(_err))

/** Log error with errno */
#define LOG_ERRNO(_fct, _err) \
	ULOGE("%s:%d: %s err=%d(%s)", __func__, __LINE__, \
			_fct, _err, strerror(_err))

/** Log error with fd and errno */
#define LOG_FD_ERRNO(_fct, _fd, _err) \
	ULOGE("%s:%d: %s(fd=%d) err=%d(%s)", __func__, __LINE__, \
			_fct, _fd, _err, strerror(_err))

/** Log error if condition failed and return from function */
#define RETURN_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			ULOGE("%s:%d: err=%d(%s)", __func__, __LINE__, \
					(_err), strerror(-(_err))); \
			return; \
		} \
	} while (0)

/** Log error if condition failed and return error from function */
#define RETURN_ERR_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			ULOGE("%s:%d: err=%d(%s)", __func__, __LINE__, \
					(_err), strerror(-(_err))); \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_err); \
		} \
	} while (0)

/** Log error if condition failed and return value from function */
#define RETURN_VAL_IF_FAILED(_cond, _err, _val) \
	do { \
		if (!(_cond)) { \
			ULOGE("%s:%d: err=%d(%s)", __func__, __LINE__, \
					(_err), strerror(-(_err))); \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_val); \
		} \
	} while (0)

/*
 * Set log level message.
 * arg1: %s : tag name.
 * arg2: %u : log level.
 */
#define ULOGCTL_MSG_ID_SET_TAG_LEV         1
#define ULOGCTL_MSG_FMT_ENC_SET_TAG_LEV    "%s%u"
#define ULOGCTL_MSG_FMT_DEC_SET_TAG_LEV    "%ms%u"

/*
 * List all tag message.
 */
#define ULOGCTL_MSG_ID_LIST_TAGS           2
#define ULOGCTL_MSG_FMT_ENC_LIST_TAGS      NULL
#define ULOGCTL_MSG_FMT_DEC_LIST_TAGS      NULL

/*
 * Tag info message.
 * arg1: %s : tag name.
 * arg2: %u : log level.
 */
#define ULOGCTL_MSG_ID_TAG_INFO            3
#define ULOGCTL_MSG_FMT_ENC_TAG_INFO       "%s%u"
#define ULOGCTL_MSG_FMT_DEC_TAG_INFO       "%ms%u"

/*
 * Tag list end message.
 */
#define ULOGCTL_MSG_ID_TAG_LIST_END        4
#define ULOGCTL_MSG_FMT_ENC_TAG_LIST_END   NULL
#define ULOGCTL_MSG_FMT_DEC_TAG_LIST_END   NULL

/*
 * Set all log level message.
 * arg1: %s : tag name.
 * arg2: %u : log level.
 */
#define ULOGCTL_MSG_ID_SET_ALL_LEV         5
#define ULOGCTL_MSG_FMT_ENC_SET_ALL_LEV    "%u"
#define ULOGCTL_MSG_FMT_DEC_SET_ALL_LEV    "%u"

#define PROCESS_SOCK_PREFIX "@ulogctl_"
#define PROCESS_SOCK_MAX_LEN 50

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_ULOGCTL_PRIV_H_ */
