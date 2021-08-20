#include <sys/time.h>
#include <unistd.h>
#include <queue>

#include "Timer.h"

// timeout using millisecond
TimerNode::TimerNode(std::shared_ptr<HttpData> requestData, int timeout)
    : deleted(false), pHttpData(requestData)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    // % 10000 ?
    expiredTime = 
        (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

TimerNode::~TimerNode()
{
    if (pHttpData)
        pHttpData->handleClose();
}

TimerNode::TimerNode(const TimerNode &tn)
    : pHttpData(tn.pHttpData), expiredTime(0) {}

void TimerNode::update(int timeout)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    expiredTime = 
        (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool TimerNode::isValid()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    size_t t = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000));

    if (t < expiredTime)
        return true;
    else {
        this->setDeleted();
        return false;
    }
}

void TimerNode::clearReq()
{
    pHttpData.reset();
    this->setDeleted();
}

void TimerManager::addTimer(std::shared_ptr<HttpData> pHttpData, int timeout)
{
    SPTimerNode newnode = std::make_shared<TimerNode>(pHttpData, timeout);
    timerNodeQueue.push(newnode);
    pHttpData->linkTimer(newnode);
}

/*
    处理逻辑
    优先队列不支持随机访问，所以被标记为 deleted 的节点会延迟到超时或者前面的节点
    均被删除，它才会被删除。
    优点：
    (1) 不需要遍历优先队列
    (2) 给超时节点一个容忍时间，可以复用RequestData节点，减少释放分配的消耗
 */

void TimerManager::handleExpiredEvent()
{
    while (!timerNodeQueue.empty()) {
        SPTimerNode ptimer_now = timerNodeQueue.top();
        
        if (ptimer_now->isDeleted() || ptimer_now->isValid() == false)     // 标记为删除或超时
            timerNodeQueue.pop();
        else
            break;

    }
}