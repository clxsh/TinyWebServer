#pragma once
#include <sys/epoll.h>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Channel.h"
#include "HttpData.h"
#include "Timer.h"

class Epoll {
public:
    Epoll();
    ~Epoll() = default;
    void epoll_add(SP_Channel request, int timeout);
    void epoll_mod(SP_Channel request, int timeout);
    void epoll_del(SP_Channel request);

    std::vector<std::shared_ptr<Channel>> poll();
    std::vector<std::shared_ptr<Channel>> getEventsRequest(int events_num);
    void add_timer(std::shared_ptr<Channel> request_data, int timeout);
    int getEpollFd()
    {
        return epollfd;
    }
    void handleExpired();

private:
    const static int MAXFDS = 100000;
    int epollfd;
    std::vector<epoll_event> events;
    std::shared_ptr<Channel> fd2chan[MAXFDS];
    std::shared_ptr<HttpData> fd2http[MAXFDS];
    TimerManager timerManager;
};