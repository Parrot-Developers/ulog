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
 */

#include <sstream>

#include <pthread.h>

#include "ulog.hpp"
#include "ulog_common.h"

namespace ulog
{
namespace internal
{

#define ULOG_CPP_BUF_SIZE 512

#define CREATE_LOGGER(_name,_level) \
	Ulogstream _name##Stream (_level, ULOG_CPP_BUF_SIZE); \
	ULOG_EXPORT OstreamUlog _name(&_name##Stream);

class Ulogstream: public std::streambuf
{
    private:
    const int   mBufSize;   // size of the buffer for one given thread
    char*       mFakeBuf;   // fake buffer of the upper class
    const int   mLevel;     // ULOG verbosity level
    static pthread_once_t   mKeyOnce;
    static pthread_key_t    mBufKey;
    static pthread_key_t    mTagKey;

    public:
    Ulogstream(int uloglevel,int bs);
    ~Ulogstream();

    virtual std::streamsize xsputn (const char* s, std::streamsize n);
    virtual int sync();
    static void keyBufDestructor(void* str);
    static void makeKeys();
    void setTag(struct ulog_cookie& c);
};

OstreamUlog::OstreamUlog(Ulogstream* p) : std::ostream(p)
{
}

// functor used to set the current tag
OstreamUlog& OstreamUlog::operator()(struct ulog_cookie& cookie)
{
    Ulogstream* p = (Ulogstream*)rdbuf();
    p->setTag(cookie);
    return *this;
}

pthread_once_t Ulogstream::mKeyOnce = PTHREAD_ONCE_INIT;
pthread_key_t  Ulogstream::mBufKey;
pthread_key_t  Ulogstream::mTagKey;

Ulogstream::Ulogstream(int uloglevel,int bs):
    std::streambuf(),
    mBufSize(bs),
    mLevel(uloglevel)
{
    // create a fake buffer to let the base class run smoothly
    mFakeBuf = (char*)malloc(mBufSize);
    setp(mFakeBuf,mFakeBuf+mBufSize);
    setg(0,0,0);
    // make sure that makeKeys is called only once within the current process
    (void)pthread_once(&mKeyOnce, &makeKeys);
}

Ulogstream::~Ulogstream()
{
    free(mFakeBuf);
}


std::streamsize Ulogstream::xsputn (const char* s, std::streamsize n) // override
{
    std::string* ptr;
    if ((ptr = (std::string*)pthread_getspecific(mBufKey)) == NULL)
    {
        // first time we get called by this thread
        // let's create a new string object
        std::string* newstr = new std::string("");
        newstr->reserve(mBufSize);
        newstr->append(s,std::min((int)n,mBufSize));
        // and attach it to the key
        (void) pthread_setspecific(mBufKey, newstr);
    }
    else
    {
        // handle buffer saturation
        int remainder = std::min((int)n,
                std::max(0,(int)(ptr->capacity()-ptr->size())));
        // append to the preexisting string
        ptr->append(s,remainder);
    }
    return n;
}

int Ulogstream::sync() // override
{
    std::string* ptr;
    ptr = (std::string*)pthread_getspecific(mBufKey);
    struct  ulog_cookie* tag = (struct  ulog_cookie*)pthread_getspecific(mTagKey);
    if (tag == NULL)
        tag = &__ulog_default_cookie;

    if (ptr != NULL && !ptr->empty())
    {
        // flush the string into ulog
        ulog_log_str(mLevel, tag, ptr->c_str());
        ptr->clear();
    }
    // reset pbase and epptr to make sure the base class
    // keeps running smoothly.
    setp(mFakeBuf,mFakeBuf+mBufSize);
    return 0;
}

// called whenever a thread exits.
void Ulogstream::keyBufDestructor(void* str)
{
    // frees the string buffer allocated for the thread
    delete (std::string*)str;
}


// create a "thread-specific data key"
void Ulogstream::makeKeys()
{
    // no need for destructor here because we use pointers
    // on the statically allocated cookie structures.
    (void)pthread_key_create(&mTagKey, NULL);
    // destructor needed since we dynamically allocate strings.
    (void)pthread_key_create(&mBufKey, &keyBufDestructor);
}

void Ulogstream::setTag(struct ulog_cookie& c)
{
    (void)pthread_setspecific(mTagKey,&c);
}

// create one logger per verbosity level
CREATE_LOGGER(UlogDraw,ULOG_DEBUG);
CREATE_LOGGER(UlogIraw,ULOG_INFO);
CREATE_LOGGER(UlogNraw,ULOG_NOTICE);
CREATE_LOGGER(UlogWraw,ULOG_WARN);
CREATE_LOGGER(UlogEraw,ULOG_ERR);
CREATE_LOGGER(UlogCraw,ULOG_CRIT);

ULOG_EXPORT NullOstreamUlog UlogNullraw;

} // namespace internal

// stream that can be used for std:cerr redirection
static
ulog::internal::Ulogstream __UlogstreamCerr(ULOG_INFO, ULOG_CPP_BUF_SIZE);
ULOG_EXPORT std::streambuf &UlogstreamCerr = __UlogstreamCerr;

} // namespace ulog
