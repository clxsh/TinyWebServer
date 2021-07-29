#pragma once

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "LogStream.h"

class AsyncLogging;

class Logger {
public:
    Logger(const char *fileName, int line);
    ~Logger();
    LogStream &stream()
    {
        return impl.stream;
    }

    static void setLogFileName(std::string fileName)
    {
        logFileName = fileName;
    }

    static std::string getLogFileName()
    {
        return logFileName;
    }

private:
    class Impl {
    public:
        Impl(const char *fileName, int _line);
        void formatTime();
        
        LogStream stream;
        int line;
        std::string basename;
    };

    Impl impl;
    static std::string logFileName;
};

#define LOG Logger(__FILE__, __LINE__).stream()