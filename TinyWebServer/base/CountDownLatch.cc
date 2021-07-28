#include "CountDownLatch.h"

CountDownLatch::CountDownLatch(int _count)
    : mutex(), condition(mutex), count(_count) {}

void CountDownLatch::wait()
{
    MutexLockGuard lock(mutex);
    
    while (count > 0)
        condition.wait();
}

void CountDownLatch::countDown()
{
    MutexLockGuard lock(mutex);
    
    --count;
    if (count == 0)
        condition.notifyAll();
}