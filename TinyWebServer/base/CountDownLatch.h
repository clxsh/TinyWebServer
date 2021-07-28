#pragma once

#include "Condition.h"
#include "MutexLock.h"
#include "noncopyable.h"

// 确保子线程启动后，start 函数再返回，防止后续对线程的操作出现问题
class CountDownLatch : noncopyable {
public:
    explicit CountDownLatch(int count);

    void wait();

    void countDown();

private:
    mutable MutexLock mutex;
    Condition condition;
    int count;
};