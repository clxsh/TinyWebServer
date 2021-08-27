#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <iostream>

#include "EventLoop.h"
#include "util.h"
#include "base/Logger.h"

using namespace std;

__thread EventLoop *t_loopInThisThread = nullptr;

int createEventfd()
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG << "Failed in eventfd";
        abort();
    }

    return evtfd;
}

EventLoop::EventLoop()
    : looping(false),
      poller(new Epoll()),
      wakeupFd(createEventfd()),
      pwakeupChannel(new Channel(this, wakeupFd)),
      quit(false),
      eventHandling(false),
      callingPendingFunctors(false),
      threadId(CurrentThread::tid())
{
    if (t_loopInThisThread) {
        LOG << "Another EventLoop exists in this thread" << threadId;
    }
    else {
        t_loopInThisThread = this;
    }

    pwakeupChannel->setEvents(EPOLLIN | EPOLLET);
    pwakeupChannel->setReadHandler(bind(&EventLoop::handleRead, this));
    pwakeupChannel->setConnHandler(bind(&EventLoop::handleConn, this));
    poller->epoll_add(pwakeupChannel, 0);
}

EventLoop::~EventLoop()
{
    close(wakeupFd);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleConn()
{
    updatePoller(pwakeupChannel, 0);
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = writen(wakeupFd, (char *)&one, sizeof(one));
    if (n != sizeof(one)) {
        LOG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
    pwakeupChannel->setEvents(EPOLLIN | EPOLLET);
}

void EventLoop::handleRead()
{
    uint64_t one;
    ssize_t n = readn(wakeupFd, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
    pwakeupChannel->setEvents(EPOLLIN | EPOLLET);
}

void EventLoop::runInLoop(Functor &&cb)
{
    if (isInLoopThread())
        cb();
    else
        queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor &&cb)
{
    {
        MutexLockGuard lock(mutex);
        pendingFunctors.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors)
        wakeup();
}

void EventLoop::loop()
{
    assert(!loop);
    assert(isInLoopThread());
    looping = true;
    quit = false;

    std::vector<SP_Channel> ret;
    while (!quit) {
        ret.clear();
        ret = poller->poll();
        eventHandling = true;
        for (auto &it : ret)
            it->handleEvents();
        eventHandling = false;
        doPendingFunctors();
        poller->handleExpired();
    }
    looping = false;
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors = true;

    {
        MutexLockGuard lock(mutex);
        functors.swap(pendingFunctors);
    }

    int fsize = functors.size();
    for (size_t i = 0; i < fsize; ++i) {
        functors[i]();
    }

    callingPendingFunctors = false;
}

void EventLoop::doQuit()
{
    quit = true;

    if (!isInLoopThread()) {
        wakeup();
    }
}