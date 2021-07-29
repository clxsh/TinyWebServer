#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "CountDownLatch.h"
#include "LogStream.h"
#include "MutexLock.h"
#include "Thread.h"
#include "noncopyable.h"

class AsyncLogging : noncopyable {
public:
    AsyncLogging(const std::string basename, int flushInterval = 2);
    ~AsyncLogging()
    {
        if (running)
            stop();
    }

    void append(const char *logline, int len);

    void start()
    {
        running = true;
        thread.start();
        latch.wait();
    }

    void stop()
    {
        running = false;    // running 的读写是否会发生竞态 ?
        cond.notify();
        thread.join();
    }

private:
    typedef FixedBuffer<kLargeBuffer> Buffer;
    typedef std::vector<std::shared_ptr<Buffer>> BufferVector;
    typedef std::shared_ptr<Buffer> BufferPtr;

    void ThreadFunc();
    const int flushInterval;
    bool running;
    std::string basename;
    Thread thread;
    MutexLock mutex;
    Condition cond;
    BufferPtr currentBuffer;
    BufferPtr nextBuffer;
    BufferVector buffers;
    CountDownLatch latch;
};