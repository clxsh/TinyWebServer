#pragma once
#include <memory>
#include <vector>

#include "EventLoopThread.h"
#include "base/Logger.h"
#include "base/noncopyable.h"

class EventLoopThreadPool : noncopyable {
public:
    EventLoopThreadPool(EventLoop *baseLoop, int numThreads);

    ~EventLoopThreadPool()
    {
        LOG << "~EventLoopThreadPool()";
    }

    void start();
    EventLoop *getNextLoop();

private:
    EventLoop *baseLoop;
    bool started;
    int numThreads;
    int next;

    std::vector<std::shared_ptr<EventLoopThread>> threads;
    std::vector<EventLoop *> loops;
};