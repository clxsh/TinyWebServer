#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *_baseLoop, int _numThreads)
    : baseLoop(_baseLoop), started(false), numThreads(_numThreads), next(0)
{
    if (numThreads <= 0) {
        LOG << "numThreads <= 0";
        abort();
    }
}

void EventLoopThreadPool::start()
{
    baseLoop->assertInLoopThread();
    started = true;

    for (int i = 0; i < numThreads; ++i) {
        std::shared_ptr<EventLoopThread> t = make_shared<EventLoopThread>();
        loops.push_back(t->startLoop());
        threads.emplace_back(move(t));        
    }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
    baseLoop->assertInLoopThread();
    assert(started);

    EventLoop *loop = baseLoop;
    if (!loops.empty()) {
        loop = loops[next];
        next = (next + 1) % numThreads;
    }

    return loop;
}