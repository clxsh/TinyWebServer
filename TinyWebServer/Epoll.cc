#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <deque>
#include <queue>
#include <iostream>

#include "Epoll.h"
#include "util.h"
#include "base/Logger.h"

using namespace std;

constexpr int EVENTSNUM = 4096;
constexpr int EPOLLWAIT_TIME = 10000;

Epoll::Epoll()
    : epollfd(epoll_create(EPOLL_CLOEXEC)), events(EVENTSNUM)
{
    assert(epollfd > 0);
}

// 注册新描述符
void Epoll::epoll_add(SP_Channel request, int timeout)
{
    int fd = request->getFd();
    if (timeout > 0) {
        add_timer(request, timeout);
        fd2http[fd] = request->getHolder();
    }

    epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents();

    request->EqualAndUpdateLastEvents();
    fd2chan[fd] = request;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) < 0) {
        perror("epoll_ctl add error");
        fd2chan[fd].reset();
        fd2http[fd].reset();
    }
}

// 修改描述符状态
void Epoll::epoll_mod(SP_Channel request, int timeout)
{
    if (timeout > 0)
        add_timer(request, timeout);
    
    int fd = request->getFd();
    if (request->EqualAndUpdateLastEvents()) {
        epoll_event event;
        event.data.fd = fd;
        event.events = request->getEvents();
        if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
            perror("epoll_mod error");
            fd2chan[fd].reset();
        }
    }
}

// 从 epoll 中删除描述符
void Epoll::epoll_del(SP_Channel request)
{
    int fd = request->getFd();
    epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents();
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event) < 0) {
        perror("epoll_ctl del error");
    }
    fd2chan[fd].reset();
    fd2http[fd].reset();
}

// 返回活跃的事件
std::vector<SP_Channel> Epoll::poll()
{
    while (true) {
        int event_count =
            epoll_wait(epollfd, &events[0], events.size(), EPOLLWAIT_TIME);
        if (event_count < 0)
            perror("epoll_wait error");

        std::vector<SP_Channel> req_data = getEventsRequest(event_count);
        if (req_data.size() > 0)
            return req_data;
    }
}

// 分发处理函数
std::vector<SP_Channel> Epoll::getEventsRequest(int events_num)
{
    std::vector<SP_Channel> req_data;

    for (int i = 0; i < events_num; ++i) {
        int fd = events[i].data.fd;
        SP_Channel cur_req = fd2chan[fd];
        if (cur_req) {
            cur_req->setRevents(events[i].events);
            cur_req->setEvents(0);
            // 分离 timer ?
            req_data.emplace_back(move(cur_req));
        }
        else {
            LOG << "SP cur_req is invalid";
        }
    }

    return req_data;
}

void Epoll::handleExpired()
{
    timerManager.handleExpiredEvent();
}

void Epoll::add_timer(SP_Channel request_data, int timeout)
{
    shared_ptr<HttpData> t = request_data->getHolder();
    if (t)
        timerManager.addTimer(t, timeout);
    else
        LOG << "timer add failed";
}