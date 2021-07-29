#pragma once

#include <memory>
#include <string>

#include "FileUtil.h"
#include "MutexLock.h"
#include "noncopyable.h"

class LogFile : noncopyable {
public:
    LogFile(const std::string &_basename, int _flushEveryN = 1024);
    ~LogFile() {};

    void append(const char *logline, int len);
    void flush();
    bool rollFile();

private:
    void append_unlocked(const char *logline, int len);

    const std::string basename;
    const int flushEveryN;

    int count;
    std::unique_ptr<MutexLock> mutex;
    std::unique_ptr<AppendFile> file;
};