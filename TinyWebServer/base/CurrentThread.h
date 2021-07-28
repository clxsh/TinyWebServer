#pragma once

#include <stdint.h>

namespace CurrentThread {
    // thread local
    extern __thread int t_cachedTid;
    extern __thread char t_tidString[32];
    extern __thread int t_tidStringLength;
    extern __thread const char *t_threadName;

    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            cacheTid();
        }

        return t_cachedTid;
    }

    // for logging
    inline const char *tidString()
    {
        return t_tidString;
    }

    // for logging
    inline int tidStringLength()
    {
        return t_tidStringLength;
    }

    inline const char *name()
    {
        return t_threadName;
    }
};