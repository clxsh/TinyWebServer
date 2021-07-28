#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "FileUtil.h"

using namespace std;

AppendFile::AppendFile(string filename) : fp(fopen(filename.c_str(), "ae"))
{
    setbuffer(fp, buffer, sizeof(buffer));
}

AppendFile::~AppendFile()
{
    fclose(fp);
}

size_t AppendFile::write(const char *logline, size_t len)
{
    return fwrite_unlocked(logline, 1, len, fp);
}

void AppendFile::append(const char *logline, const size_t len)
{
    size_t nwritten = 0;
    size_t remain = len;

    while (remain > 0) {
        size_t ret = this->write(logline + nwritten, remain);
        if (ret == 0) {
            int err = ferror(fp);
            if (err)
                fprintf(stderr, "AppendFile::append() failed!\n");
            break;
        }

        nwritten += ret;
        remain = remain - ret;
    }
}

void AppendFile::flush()
{
    fflush(fp);
}