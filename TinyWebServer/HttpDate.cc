#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>

#include "HttpData.h"
#include "Channel.h"
#include "EventLoop.h"
#include "util.h"

using namespace std;

pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;
std::unordered_map<std::string, std::string> MimeType::mime;

const uint32_t DEFAULT_EVENT = EPOLLIN | EPOLLET | EPOLLONESHOT;
const int DEFAULT_EXPIRED_TIME = 2000;              // ms
const int DEFAULT_KEEP_ALIVE_TIME = 5 * 60 * 1000;  // ms

void MimeType::init()
{
    mime[".html"] = "text/html";
    mime[".avi"] = "video/x-msvideo";
    mime[".bmp"] = "image/bmp";
    mime[".c"] = "text/plain";
    mime[".doc"] = "application/msword";
    mime[".gif"] = "image/gif";
    mime[".gz"] = "application/x-gzip";
    mime[".htm"] = "text/html";
    mime[".ico"] = "image/x-icon";
    mime[".jpg"] = "image/jpeg";
    mime[".png"] = "image/png";
    mime[".txt"] = "text/plain";
    mime[".mp3"] = "audio/mp3";
    mime["default"] = "text/html";
}

std::string MimeType::getMime(const std::string &suffix)
{
    pthread_once(&once_control, MimeType::init);
    if (mime.find(suffix) == mime.end()) {
        return mime["default"];
    }
    else {
        return mime[suffix];
    }
}

HttpData::HttpData(EventLoop *_loop, int connfd)
    : loop(_loop),
      channel(new Channel(loop, connfd)),
      fd(connfd),
      error(false),
      connectionState(H_CONNECTED),
      method(METHOD_GET),
      httpVersion(HTTP_11),
      nowReadPos(0),
      state(STATE_PARSE_URI),
      hState(H_START),
      keepAlive(false)
{
    channel->setReadHandler(bind(&HttpData::handleRead, this));
    channel->setWritehandler(bind(&HttpData::handleWrite, this));
    channel->setConnHandler(bind(&HttpData::handleConn, this));
}

void HttpData::reset()
{
    fileName.clear();
    path.clear();
    nowReadPos = 0;
    state = STATE_PARSE_URI;
    hState = H_START;
    headers.clear();

    auto my_timer = timer.lock();
    if (my_timer) {
        my_timer->clearReq();
        timer.reset();
    }
}

