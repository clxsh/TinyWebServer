#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <functional>

#include "AsyncLogging.h"
#include "LogFile.h"

AsyncLogging::AsyncLogging(std::string logFileName, int _flushInterval)
    : flushInterval(_flushInterval),
      running(false),
      basename(logFileName),
      thread(std::bind(&AsyncLogging::ThreadFunc, this), "Logging"),
      mutex(),
      cond(mutex),
      currentBuffer(std::make_shared<Buffer>()),
      nextBuffer(std::make_shared<Buffer>()),
      buffers(),
      latch(1)
{
    assert(logFileName.size() > 1);
    currentBuffer->bzero();
    nextBuffer->bzero();
    buffers.reserve(16);
}

void AsyncLogging::append(const char *logline, int len)
{
    MutexLockGuard lock(mutex);
    
    if (currentBuffer->avail() > len) {
        currentBuffer->append(logline, len);
    }
    else {
        /*
            buffers_.push_back(currentBuffer_);
            currentBuffer_.reset();
            if (nextBuffer_)
                currentBuffer_ = std::move(nextBuffer_);
            else
                currentBuffer_.reset(new Buffer);

            这里对原代码进行了优化
         */
        buffers.emplace_back(std::move(currentBuffer));
        if (nextBuffer)
            currentBuffer = std::move(nextBuffer);
        else
            currentBuffer = std::make_shared<Buffer>();
        
        currentBuffer->append(logline, len);
        cond.notify();
    }
}

/*
 * 将日志写入到文件
 * (可能发生很多的日志丢弃, 没关系吗 ?) 缓冲似乎足够大。。
 */
void AsyncLogging::ThreadFunc()
{
    assert(running);
    latch.countDown();
    
    LogFile output(basename);
    BufferPtr newBuffer1 = std::make_shared<Buffer>();
    BufferPtr newBuffer2 = std::make_shared<Buffer>();
    newBuffer1->bzero();
    newBuffer2->bzero();
    
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    
    while (running) {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);

        // 进入临界区，对 currentBuffer 中的区域添加到 buffers 中, 并与 buffersToWrite 交换
        // 将 newBuffer1 管理的 Buffer 转给 currentBuffer
        {
            MutexLockGuard lock(mutex);
            if (buffers.empty()) {
                cond.waitForSeconds(flushInterval);      // 万一到时间也没有 log ? 还是说不可能啊
            }
            buffers.emplace_back(std::move(currentBuffer));
            currentBuffer = std::move(newBuffer1);
            buffersToWrite.swap(buffers);
            
            if (!nextBuffer)
                nextBuffer = std::move(newBuffer2);
        }

        assert(!buffersToWrite.empty());

        if (buffersToWrite.size() > 25) {
            // 只剩两个, 其余 buffer 删掉 
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }

        for (size_t i = 0; i < buffersToWrite.size(); ++i) {
            output.append(buffersToWrite[i]->getData(), buffersToWrite[i]->length());
        }

        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }

        if (!newBuffer1) {
            assert(!buffersToWrite.empty());
            newBuffer1 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer1->reset();     // call class Buffer's reset(), instead of shared_ptr
        }

        if (!newBuffer2) {
            assert(!buffersToWrite.empty());
            newBuffer2 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
        output.flush();
    }

    output.flush();
}