#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include "Timer.h"

class EventLoop;
class TimerNode;
class Channel;

enum ProcessState {
    STATE_PARSE_URI = 1,
    STATE_PARSE_HEADERS,
    STATE_RECV_BODY,
    STATE_ANALYSIS,
    STATE_FINISH
};

enum URIState {
    PARSE_URI_AGAIN = 1,
    PARSE_URI_ERROR,
    PARSE_URI_SUCCESS
};

enum HeaderState {
    PARSE_HEADER_SUCCESS = 1,
    PARSE_HEADER_AGAIN,
    PARSE_HEADER_ERROR
};

enum AnalysisState {
    ANALYSIS_SUCCESS = 1,
    ANALYSIS_ERROR
};

enum ParseState {
    H_START = 0,
    H_KEY,
    H_COLON,
    H_SPACES_AFTER_COLON,
    H_VALUE,
    H_CR,
    H_LF,
    H_END_CR,
    H_END_LF
};

enum ConnectionState {
    H_CONNECTED = 0,
    H_DISCONNECTING,
    H_DISCONNECTED
};

enum HttpMethod {
    METHOD_POST = 1,
    METHOD_GET,
    METHOD_HEAD
};

enum HttpVersion {
    HTTP_10 = 1,
    HTTP_11,
};

class MimeType {
private:
    static void init();
    static std::unordered_map<std::string, std::string> mime;
    MimeType();
    MimeType(const MimeType &m);

public:
    static std::string getMime(const std::string &suffix);

private:
    static pthread_once_t once_control;
};

// 需注意 shared from this 之前一定要有一个控制块, 否则行为未定义
class HttpData : public std::enable_shared_from_this<HttpData> {
public:
    HttpData(EventLoop *loop, int connfd);
    ~HttpData() { close(fd); }

    void reset();
    void seperateTimer();

    void linkTimer(std::shared_ptr<TimerNode> mtimer)
    {
        timer = mtimer;
    }

    std::shared_ptr<Channel> getChannel() { return channel; }
    EventLoop *getLoop() { return loop; }
    void handleClose();
    void newEvent();

private:
    EventLoop *loop;
    std::shared_ptr<Channel> channel;
    int fd;
    std::string inBuffer;
    std::string outBuffer;
    bool error;
    
    ConnectionState connectionState;
    HttpMethod method;
    HttpVersion httpVersion;
    std::string fileName;
    std::string path;
    int nowReadPos;
    ProcessState state;
    ParseState hState;
    bool keepAlive;
    std::map<std::string, std::string> headers;
    std::weak_ptr<TimerNode> timer;

    void handleRead();
    void handleWrite();
    void handleConn();
    void handleError(int fd, int err_num, std::string short_msg);

    URIState parseURI();
    HeaderState parseHeaders();
    AnalysisState analysisRequest();
};