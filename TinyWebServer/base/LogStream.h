#pragma once

#include <assert.h>
#include <string.h>
#include <string>

#include "noncopyable.h"

class AsyncLogging;
constexpr int kSmallBuffer = 4000;
constexpr int kLargeBuffer = 4000 * 1000;

template <int SIZE>
class FixedBuffer : noncopyable {
public:
    FixedBuffer() : cur(data) {}
    ~FixedBuffer() {}

    void append(const char *buf, size_t len)
    {
        if (avail() > static_cast<int>(len)) {
            memcpy(cur, buf, len);
            cur += len;
        }
    }

    const char *getData() const
    {
        return data;
    }

    // return data length
    int length() const
    {
        return static_cast<int>(cur - data);
    }

    char *current()
    {
        return cur;
    }

    // return available space
    int avail() const
    {
        return static_cast<int>(end() - cur);
    }

    void add(size_t len)
    {
        cur += len;
    }

    void reset()
    {
        cur = data;
    }

    void bzero()
    {
        memset(data, 0, sizeof(data));
    }

private:
    const char *end() const
    {
        return data + sizeof(data);
    }

    char data[SIZE];
    char *cur;
};

class LogStream : noncopyable {
public:
    typedef FixedBuffer<kSmallBuffer> Buffer;

    LogStream &operator<<(bool v)
    {
        buffer.append(v ? "1" : "0", 1);
        return *this;
    }

    LogStream &operator<<(short);
    LogStream &operator<<(unsigned short);
    LogStream &operator<<(int);
    LogStream &operator<<(unsigned int);
    LogStream &operator<<(long);
    LogStream &operator<<(unsigned long);
    LogStream &operator<<(long long);
    LogStream &operator<<(unsigned long long);

    LogStream &operator<<(float v)
    {
        *this << static_cast<double>(v);
        
        return *this;
    }
    LogStream &operator<<(double);
    LogStream &operator<<(long double);

    LogStream &operator<<(char v)
    {
        buffer.append(&v, 1);
        
        return *this;
    }
    
    LogStream &operator<<(const char *str)
    {
        if (str)
            buffer.append(str, strlen(str));
        else
            buffer.append("(null)", 6);

        return *this;
    }

    LogStream &operator<<(const unsigned char *str)
    {
        return operator<<(reinterpret_cast<const char *>(str));
    }

    LogStream &operator<<(const std::string &v)
    {
        buffer.append(v.c_str(), v.size());

        return *this;
    }

    LogStream &operator<<(const void *);

    void append(const char *data, int len)
    {
        buffer.append(data, len);
    }

    const Buffer &getBuffer() const
    {
        return buffer;
    }

    void resetBuffer()
    {
        buffer.reset();
    }
private:
    void staticCheck();

    template <typename T>
    void formatInteger(T);

    Buffer buffer;

    static constexpr int kMaxNumericSize = 32;
};