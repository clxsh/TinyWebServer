#pragma once
#include <memory>

#include "Channel.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"

class Server {
public:
    Server(EventLoop *loop, int threadNum, int port);
    ~Server() {}

    EventLoop *getLoop() const
    {
        return loop;
    }

    void start();
    void handleNewConn();
    
    void handleThisConn()
    {
        loop->updatePoller(acceptChannel);    
    }

private:
    EventLoop *loop;
    int threadNum;
    bool started;
    int port;
    int listenfd;

    std::unique_ptr<EventLoopThreadPool> eltp;
    std::shared_ptr<Channel> acceptChannel;
    
    static const int MAXFDS = 100000;
};