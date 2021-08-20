#pragma once

#include <unistd.h>
#include <deque>
#include <memory>
#include <queue>
#include "HttpData.h"
#include "base/MutexLock.h"
#include "base/noncopyable.h"

class HttpData;

class TimerNode {
public:
    TimerNode(std::shared_ptr<HttpData> requestData, int timeout);
    ~TimerNode();
    TimerNode(const TimerNode &tn);
    void update(int timeout);
    bool isValid();
    void clearReq();
    void setDeleted() { deleted = true; }
    bool isDeleted() const { return deleted; }
    size_t getExpiredTime() const { return expiredTime; }

private:
    bool deleted;
    size_t expiredTime;
    std::shared_ptr<HttpData> pHttpData;
};

struct TimerCmp {
    bool operator()(std::shared_ptr<TimerNode> &a,
                    std::shared_ptr<TimerNode> &b) const
    {
        return a->getExpiredTime() > b->getExpiredTime();
    }
};

class TimerManager {
public:
    TimerManager() = default;
    ~TimerManager() = default;
    void addTimer(std::shared_ptr<HttpData> pHttpData, int timeout);
    void handleExpiredEvent();

private:
    typedef std::shared_ptr<TimerNode> SPTimerNode;
    std::priority_queue<SPTimerNode, std::deque<SPTimerNode>, TimerCmp>
        timerNodeQueue;
};