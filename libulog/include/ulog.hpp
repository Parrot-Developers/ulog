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
 *
 * @brief   c++ wrapper for libulog
 *
 *   Usage:
 *   UlogI<<"this is my vector:"<<Eigen::Vector3d(1,2,3)<<endl;
 *
 *   Benchmark:
 *   Direct call to ULOG versus c++ encapsulation
 *   Test platform: PC Linux 32bit
 *   Test case: write 1 million times "01234567890123456789012345 "<loop_idx> into the log.
 *   Result:
 *       ULOG: real 0.570s
 *       c++ : real 0.594s
 *   Conclusion: at least in that case, C++ wrapper is nearly as fast as C API.
 */

#ifndef _PARROT_ULOG_CPP_H
#define _PARROT_ULOG_CPP_H

#include <iostream>

#include "ulog.h"

#define UlogD   ulog::internal::UlogDraw(__ULOG_COOKIE)
#define UlogI   ulog::internal::UlogIraw(__ULOG_COOKIE)
#define UlogN   ulog::internal::UlogNraw(__ULOG_COOKIE)
#define UlogW   ulog::internal::UlogWraw(__ULOG_COOKIE)
#define UlogE   ulog::internal::UlogEraw(__ULOG_COOKIE)
#define UlogC   ulog::internal::UlogCraw(__ULOG_COOKIE)

namespace ulog
{

extern std::streambuf &UlogstreamCerr;

namespace internal
{

// forward declaration
class Ulogstream;


class OstreamUlog : public std::ostream
{
    public:
    OstreamUlog(Ulogstream* p);
    OstreamUlog& operator()(struct ulog_cookie& cookie) __attribute__ ((visibility ("default")));
};


extern OstreamUlog UlogDraw;
extern OstreamUlog UlogIraw;
extern OstreamUlog UlogNraw;
extern OstreamUlog UlogWraw;
extern OstreamUlog UlogEraw;
extern OstreamUlog UlogCraw;

}
}
#endif
