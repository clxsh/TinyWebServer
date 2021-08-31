#pragma once
#include <sys/epoll.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Timer.h"

class EventLoop;
class HttpData;

class Channel {
    typedef std::function<void()> CallBack;

private:
    // data
    EventLoop *loop;
    int fd;
    uint32_t events;
    uint32_t revents;
    uint32_t lastEvents;

    std::weak_ptr<HttpData> holder;

private:
    CallBack readHandler;
    CallBack writeHandler;
    CallBack errorHandler;
    CallBack connHandler;

public:
    Channel(EventLoop *_loop)
        : loop(_loop), events(0), lastEvents(0), fd(0) {}

    Channel(EventLoop *_loop, int _fd)
        : loop(_loop), fd(_fd), events(0), lastEvents(0) {}
    ~Channel() = default;

    int getFd()
    {
        return fd;
    }
    void setFd(int _fd)
    {
        fd = _fd;
    }

    void setHolder(std::shared_ptr<HttpData> _holder)
    {
        holder = _holder;
    }
    auto getHolder()
    {
        return holder.lock();
    }

    void setReadHandler(CallBack &&_readHandler)
    {
        readHandler = _readHandler;
    }
    void setWriteHandler(CallBack &&_writeHandler)
    {
        writeHandler = _writeHandler;
    }
    void setErrorHandler(CallBack &&_errorHandler)
    {
        errorHandler = _errorHandler;
    }
    void setConnHandler(CallBack &&_connHandler)
    {
        connHandler = _connHandler;
    }

    // 分发处理事件
    void handleEvents()
    {
        events = 0;
        if ((revents & EPOLLHUP) && !(revents & EPOLLIN)) {
            events = 0;
            return;
        }

        if (revents & EPOLLERR) {
            if (errorHandler)
                errorHandler();
            events = 0;
            return;
        }

        if (revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
            handleRead();
        }

        if (revents & EPOLLOUT) {
            handleWrite();
        }

        handleConn();
    }

    void handleRead()
    {
        if (readHandler)
            readHandler();
    }

    void handleWrite()
    {
        if (writeHandler)
            writeHandler();
    }

    void handleConn()
    {
        if (connHandler)
            connHandler();
    }

    void setRevents(uint32_t ev)
    {
        revents = ev;
    }

    void setEvents(uint32_t ev)
    {
        events = ev;
    }

    uint32_t getEvents()
    {
        return events;
    }

    bool EqualAndUpdateLastEvents()
    {
        bool ret = (lastEvents == events);
        lastEvents = events;

        return ret;
    }

    uint32_t getLastEvents()
    {
        return lastEvents;
    }
};

typedef std::shared_ptr<Channel> SP_Channel;