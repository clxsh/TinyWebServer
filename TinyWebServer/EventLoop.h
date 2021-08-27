#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <iostream>

#include "Channel.h"
#include "Epoll.h"
#include "util.h"
#include "base/CurrentThread.h"
#include "base/Logger.h"
#include "base/Thread.h"

using namespace std;

class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void doQuit();
    void runInLoop(Functor &&cb);
    void queueInLoop(Functor &&cb);
    
    bool isInLoopThread() const
    {
        return threadId == CurrentThread::tid();
    }

    void assertInLoopThread()
    {
        assert(isInLoopThread());
    }

    void shutdown(shared_ptr<Channel> channel)
    {
        shutdown_wr(channel->getFd());
    }

    void removeFromPoller(shared_ptr<Channel> channel)
    {
        poller->epoll_del(channel);
    }

    void updatePoller(shared_ptr<Channel> channel, int timeout = 0)
    {
        poller->epoll_mod(channel, timeout);
    }

    void addToPoller(shared_ptr<Channel> channel, int timeout = 0)
    {
        poller->epoll_add(channel, timeout);
    }

private:
    // 如何处理的这几个变量的竞态访问问题 ?
    bool looping;
    bool quit;
    bool eventHandling;
    bool callingPendingFunctors;

    shared_ptr<Epoll> poller;
    std::vector<Functor> pendingFunctors;

    int wakeupFd;
    shared_ptr<Channel> pwakeupChannel;

    mutable MutexLock mutex;
    const pid_t threadId;
    
    void wakeup();
    void handleRead();
    void doPendingFunctors();
    void handleConn();
};