#pragma once

#include <string>
#include "noncopyable.h"

static constexpr int BUF_SIZE = 64 * 1024;

class AppendFile : noncopyable {
public:
    explicit AppendFile(std::string filename);
    ~AppendFile();

    void append(const char *logline, const size_t len);
    void flush();

private:
    size_t write(const char *logline, size_t len);
    FILE *fp;
    char buffer[BUF_SIZE];
};