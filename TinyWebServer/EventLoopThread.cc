#include <functional>
#include "EventLoopThread.h"

EventLoopThread::EventLoopThread()
    : loop(nullptr),
      exiting(false),
      thread(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"),
      mutex(),
      cond(mutex) {}

EventLoopThread::~EventLoopThread()
{
    exiting = false;
    if (loop != nullptr) {
        loop->doQuit();
        thread.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    assert(!thread.isStarted());
    thread.start();
    {
        MutexLockGuard lock(mutex);
        while (loop == nullptr)
            cond.wait();
    }

    return loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop tloop;

    {
        MutexLockGuard lock(mutex);
        loop = &tloop;
        cond.notify();
    }

    tloop.loop();
    loop = nullptr;
}