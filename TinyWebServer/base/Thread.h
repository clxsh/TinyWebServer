#pragma once

#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <functional>
#include <memory>
#include <string>

#include "CountDownLatch.h"
#include "noncopyable.h"

class Thread : noncopyable {
public:
    typedef std::function<void()> ThreadFunc;
    
    explicit Thread(const ThreadFunc &, const std::string &name = std::string());
    ~Thread();
    
    void start();
    int join();
    
    bool started() const
    {
        return started;
    }

    pid_t getTid() const
    {
        return tid;
    }

    const std::string &getName() const
    {
        return name;
    }

private:
    void setDefaultName();
    bool started;
    bool joined;
    pthread_t pthreadId;
    pid_t tid;
    ThreadFunc func;
    std::string name;
    CountDownLatch latch;
};