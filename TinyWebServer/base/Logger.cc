#include <assert.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>

#include "Logger.h"
#include "CurrentThread.h"
#include "Thread.h"
#include "AsyncLogging.h"

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static AsyncLogging *asyncLogging;

std::string Logger::logFileName = "./server.log";

void once_init()
{
    asyncLogging = new AsyncLogging(Logger::getLogFileName());
    asyncLogging->start();
}

void output(const char *msg, int len)
{
    pthread_once(&once_control, once_init);
    asyncLogging->append(msg, len);
}

Logger::Impl::Impl(const char *fileName, int _line)
    : stream(),
      line(_line),
      basename(fileName)
{
    formatTime();
}

void Logger::Impl::formatTime()
{
    struct timeval tv;
    time_t time;
    char str_t[26] = {0};
    gettimeofday(&tv, NULL);
    time = tv.tv_sec;
    struct tm *p_time = localtime(&time);
    strftime(str_t, 26, "%Y-%m-%d %H:%M:%S\n", p_time);
    stream << str_t;
}

Logger::Logger(const char *fileName, int line)
    : impl(fileName, line)
{}

Logger::~Logger()
{
    impl.stream << " -- " << impl.basename << ":" << impl.line << "\n\n";
    const LogStream::Buffer &buf = stream().getBuffer();
    output(buf.getData(), buf.length());
}