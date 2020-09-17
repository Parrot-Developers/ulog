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
 */

#include <string>

#include <stdint.h>

#define  ULOG_TAG pulsarsoca
#include "ulog.hpp"
#include "ulog_stdcerr.h"

ULOG_DECLARE_TAG(pulsarsoca);
ULOG_DECLARE_TAG(foo);

#define NBLOOP      10000
#define NBTHREADS   100

using namespace std;

namespace ulogtest {
    static void bar(void)
    {
        ULOGI("hello from %s", __func__);
    }
}

void* entry_point (void * arg)
{
    int id = (int)(intptr_t)(arg);
    for (int i=0;i<NBLOOP;i++)
    {
        UlogI<<"From thread"<<id<<"\n";
        UlogI<<"printing "<<id<<"..."<<endl;
    }
    return NULL;
}



int main(int argc, char* argv[])
{
    ULOGN("Logging from C++ code...");
    ulogtest::bar();

    pthread_t mythread[NBTHREADS];
    for (int j=0;j<NBTHREADS;j++)
    {
        (void)pthread_create(&mythread[j], NULL, &entry_point,
                             (void*)(intptr_t)j);
    }

    // special case worth being tested
    UlogN<<endl;

    for (int j=0;j<NBTHREADS;j++)
        pthread_join(mythread[j], NULL);

    UlogD<<"this is debug verbosity level"<<endl;
    UlogN<<"this is notice verbosity level"<<endl;
    UlogW<<"this is warning verbosity level"<<endl;
    UlogE<<"this is error verbosity level"<<endl;
    UlogC<<"this is critical verbosity level"<<endl;

    ulog_stdcerr_redirect();
    std::cerr << "this is a std::cerr log" << endl << "2nd log" << endl;

    UlogNull << "this shall do nothing" << endl;

    return 0;
}
