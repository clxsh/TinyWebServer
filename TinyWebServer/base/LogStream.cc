#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <limits>

#include "LogStream.h"

const char digits[] = "9876543210123456789";
const char *zero = digits + 9;

// ensure buf size enough to store value
template <typename T>
size_t convert(char buf[], T value)
{
    T i = value;
    char *p = buf;

    do {
        int lsd = static_cast<int>(i % 10);
        i /= 10;
        *p++ = zero[lsd];
    } while (i != 0);

    if (value < 0) {
        *p++ = '-';
    }
    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}

template <typename T>
void LogStream::formatInteger(T v)
{
    // buffer 不够 kMaxNumericSize 个字符直接丢弃
    if (buffer.avail() >= kMaxNumericSize) {
        size_t len = convert(buffer.current(), v);
        buffer.add(len);
    }
}

LogStream &LogStream::operator<<(short v)
{
    *this << static_cast<int>(v);

    return *this;
}

LogStream &LogStream::operator<<(unsigned short v)
{
    *this << static_cast<unsigned int>(v);

    return *this;
}

LogStream &LogStream::operator<<(int v)
{
    formatInteger(v);

    return *this;
}

LogStream &LogStream::operator<<(unsigned int v)
{
    formatInteger(v);

    return *this;
}

LogStream& LogStream::operator<<(long v) {
  formatInteger(v);

  return *this;
}

LogStream& LogStream::operator<<(unsigned long v) {
  formatInteger(v);

  return *this;
}

LogStream& LogStream::operator<<(long long v) {
  formatInteger(v);

  return *this;
}

LogStream& LogStream::operator<<(unsigned long long v) {
  formatInteger(v);

  return *this;
}

LogStream &LogStream::operator<<(double v)
{
    if (buffer.avail() >= kMaxNumericSize) {
        int len = snprintf(buffer.current(), kMaxNumericSize, "%.12g", v);
        buffer.add(len);
    }

    return *this;
}

LogStream &LogStream::operator<<(long double v)
{
    if (buffer.avail() >= kMaxNumericSize) {
        int len = snprintf(buffer.current(), kMaxNumericSize, "%.12Lg", v);
        buffer.add(len);
    }

    return *this;
}