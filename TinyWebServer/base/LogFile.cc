#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "LogFile.h"
#include "FileUtil.h"

using namespace std;

LogFile::LogFile(const string &_basename, int _flushEveryN)
    : basename(_basename),
      flushEveryN(_flushEveryN),
      count(0),
      mutex(make_unique<MutexLock>()),
      file(make_unique<AppendFile>(basename)) {}

void LogFile::append(const char *logline, int len)
{
    MutexLockGuard lock(*mutex);
    append_unlocked(logline, len);
}

void LogFile::flush()
{
    MutexLockGuard lock(*mutex);
    file->flush();
}

void LogFile::append_unlocked(const char *logline, int len)
{
    file->append(logline, len);
    ++count;
    if (count >= flushEveryN) {
        count = 0;
        file->flush();
    }
}