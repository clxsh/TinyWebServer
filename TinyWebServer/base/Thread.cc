#include <assert.h>
#include <errno.h>
#include <linux/unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory>

#include "Thread.h"
#include "CurrentThread.h"

#include <iostream>
using namespace std;

// auxiliary function
pid_t gettid()
{
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

// namespace CurrentThread
namespace CurrentThread {
    __thread int t_cachedTid = 0;
    __thread char t_tidString[32];
    __thread int t_tidStringLength = 6;
    __thread const char *t_threadName = "default";
};

void CurrentThread::cacheTid()
{
    if (t_cachedTid == 0) {
        t_cachedTid = gettid();
        t_tidStringLength =
            snprintf(t_tidString, sizeof(t_tidString), "%5d ", t_cachedTid);
    }
}
// namespace CurrentThread.

struct ThreadData {
    typedef Thread::ThreadFunc ThreadFunc;

    ThreadFunc func;
    string name;
    pid_t *tid;
    CountDownLatch *latch;

    ThreadData(const ThreadFunc &_func, const string &_name, pid_t *_tid,
               CountDownLatch *_latch)
        : func(_func), name(_name), tid(_tid), latch(_latch) {}

    void runInThread()
    {
        *tid = CurrentThread::tid();
        tid = nullptr;
        latch->countDown();
        latch = nullptr;

        CurrentThread::t_threadName = name.empty() ? "Thread" : name.c_str();
        prctl(PR_SET_NAME, CurrentThread::t_threadName);

        func();
        CurrentThread::t_threadName = "finished";
    }
};

void *startThread(void *obj)
{
    ThreadData *data = static_cast<ThreadData *>(obj);
    data->runInThread();
    delete data;

    return nullptr;
}

Thread::Thread(const ThreadFunc &_func, const string &_name)
    : started(false),
      joined(false),
      pthreadId(0),
      tid(0),
      func(_func),
      name(_name),
      latch(1)
{
    // setDefaultName();
}

Thread::~Thread()
{
    if (started && !joined)
        pthread_detach(pthreadId);
}

void Thread::setDefaultName()
{
    if (name.empty()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread");
        name = buf;
    }
}

void Thread::start()
{
    assert(!started);
    started = true;

    ThreadData *data = new ThreadData(func, name, &tid, &latch);

    if (pthread_create(&pthreadId, NULL, &startThread, data) != 0) {
        started = false;
        delete data;
    }
    else {
        latch.wait();
        assert(tid > 0);
    }
}

int Thread::join()
{
    assert(started);
    assert(!joined);
    joined = true;

    return pthread_join(pthreadId, NULL);
}