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
    channel->setWriteHandler(bind(&HttpData::handleWrite, this));
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

void HttpData::seperateTimer()
{
    auto my_timer = timer.lock();
    if (my_timer) {
        my_timer->clearReq();
        timer.reset();
    }
}

void HttpData::handleRead()
{
    uint32_t events = channel->getEvents();

    do {
        bool read_zero = false;
        int read_num = readn(fd, inBuffer, read_zero);
        LOG << "Request: " << inBuffer;

        // 连接断开
        if (connectionState == H_DISCONNECTING) {
            inBuffer.clear();
            break;
        }

        // read error
        if (read_num < 0) {
            perror("read error");
            error = true;
            handleError(fd, 400, "Bad Request");
            break;
        }
        else if (read_zero) {
            // 读取到0, 可能对端关闭了写或断开了连接
            connectionState = H_DISCONNECTING;
            if (read_num == 0) {
                break;
            }
        }

        if (state == STATE_PARSE_URI) {
            URIState uState = this->parseURI();
            if (uState == PARSE_URI_AGAIN)
                break;
            else if (uState == PARSE_URI_ERROR) {
                perror("parse URI error");
                LOG << "FD = " << fd << "," << inBuffer << "******";
                inBuffer.clear();
                error = true;
                handleError(fd, 400, "Bad Request");
                break;
            }
            else {
                state = STATE_PARSE_HEADERS;
            }
        }

        if (state == STATE_PARSE_HEADERS) {
            HeaderState hState = this->parseHeaders();
            if (hState == PARSE_HEADER_AGAIN)
                break;
            else if (hState == PARSE_HEADER_ERROR) {
                perror("parse header error");
                error = true;
                handleError(fd, 400, "Bad Request");
                break;
            }

            if (method == METHOD_POST) {
                state = STATE_RECV_BODY;
            }
            else {
                state = STATE_ANALYSIS;
            }
        }

        if (state == STATE_RECV_BODY) {
            int content_length = -1;
            auto it = headers.find("Content-length");
            if (it != headers.end()) {
                content_length = stoi(it->second);
            }
            else {
                error = true;
                handleError(fd, 400, "Bad Request: Lack of argument (Content-length)");
                break;
            }
            if (static_cast<int>(inBuffer.size()) < content_length)
                break;
            state = STATE_ANALYSIS;
        }

        if (state == STATE_ANALYSIS) {
            AnalysisState aState = this->analysisRequest();
            if (aState == ANALYSIS_SUCCESS) {
                state = STATE_FINISH;
                break;
            }
            else {
                error = true;
                break;
            }
        }
    } while (false);

    if (!error) {
        if (outBuffer.size() > 0) {
            handleWrite();
        }

        if (!error && state == STATE_FINISH) {
            this->reset();
            if (inBuffer.size() > 0) {
                if (connectionState != H_DISCONNECTING)
                    handleRead();
            }
        }
        else if (!error && connectionState != H_DISCONNECTED) {
            channel->setEvents(events | EPOLLIN);
        }
    }
}

void HttpData::handleWrite()
{
    if (!error && connectionState != H_DISCONNECTED) {
        uint32_t events = channel->getEvents();
        if (writen(fd, outBuffer) < 0) {
            perror("writen");
            channel->setEvents(0);
            error = true;
        }
        // 未发送完毕
        if (outBuffer.size() > 0)
            channel->setEvents(events | EPOLLOUT);
    }
}

void HttpData::handleConn()
{
    seperateTimer();
    uint32_t events = channel->getEvents();

    if (!error && connectionState == H_CONNECTED) {
        if (events != 0) {
            int timeout = DEFAULT_EXPIRED_TIME;
            if (keepAlive)
                timeout = DEFAULT_KEEP_ALIVE_TIME;
            if ((events & EPOLLIN) && (events & EPOLLOUT)) {
                channel->setEvents(EPOLLOUT);
            }
            channel->setEvents(channel->getEvents() | EPOLLET);
            loop->updatePoller(channel, timeout);
        }
        else if (keepAlive) {
            channel->setEvents(events | EPOLLIN | EPOLLET);
            int timeout = DEFAULT_KEEP_ALIVE_TIME;
            loop->updatePoller(channel, timeout);
        }
        else {
            channel->setEvents(events | EPOLLIN | EPOLLET);
            int timeout = (DEFAULT_KEEP_ALIVE_TIME >> 1);
            loop->updatePoller(channel, timeout);
        }
    }
    else if (!error && connectionState == H_DISCONNECTING &&
             (events & EPOLLOUT)) {
        channel->setEvents(EPOLLOUT | EPOLLET);
    }
    else {
        loop->runInLoop(bind(&HttpData::handleClose, shared_from_this()));
    }
}

// 解析 HTTP 请求首行
URIState HttpData::parseURI()
{
    string &str = inBuffer;
    
    // 读一个完整行
    size_t pos = str.find('\r', nowReadPos);
    if (pos < 0) {
        return PARSE_URI_AGAIN;
    }

    // 取出首行
    string request_line = str.substr(0, pos);
    if (str.size() > pos + 1) {
        str = str.substr(pos + 1);
    }
    else {
        str.clear();
    }

    // method
    int posGet = request_line.find("GET");
    int posPost = request_line.find("POST");
    int posHead = request_line.find("HEAD");

    if (posGet >= 0) {
        pos = posGet;
        method = METHOD_GET;
    }
    else if (posPost >= 0) {
        pos = posPost;
        method = METHOD_POST;
    }
    else if (posHead >= 0) {
        pos = posHead;
        method = METHOD_HEAD;
    }
    else {
        return PARSE_URI_ERROR;
    }

    // 
    pos = request_line.find("/", pos);
    if (pos < 0) {
        fileName = "index.html";
        httpVersion = HTTP_11;

        return PARSE_URI_SUCCESS;
    }
    else {
        size_t pos2 = request_line.find(' ', pos);
        if (pos2 < 0) {
            return PARSE_URI_ERROR;
        }
        else {
            if (pos2 - pos > 1) {
                fileName = request_line.substr(pos + 1, pos2 - pos + 1);
                size_t pos3 = fileName.find('?');
                if (pos3 > 0) {
                    fileName = fileName.substr(0, pos3);
                }
            }
            else {
                fileName = "index.html";
            }
        }
        pos = pos2;
    }

    // HTTP 版本号
    pos = request_line.find("/", pos);
    if (pos < 0)
        return PARSE_URI_ERROR;
    else {
        if (request_line.size() - pos <= 3)
            return PARSE_URI_ERROR;
        else {
            string ver = request_line.substr(pos + 1, 3);
            if (ver == "1.0")
                httpVersion = HTTP_10;
            else if (ver == "1.1")
                httpVersion = HTTP_11;
            else
                return PARSE_URI_ERROR;
        }
    }

    return PARSE_URI_SUCCESS;
}

// 解析请求头
HeaderState HttpData::parseHeaders()
{
    string &str = inBuffer;
    int key_start = -1;
    int key_end = -1;
    int value_start = -1;
    int value_end = -1;
    int now_read_line_begin = 0;
    bool notFinish = true;
    
    size_t i = 0;
    for (; i < str.size() && notFinish; ++i) {
        switch (hState) {
        case H_START: {
            if (str[i] == '\n' || str[i] == '\r') break;
            hState = H_KEY;
            key_start = i;
            now_read_line_begin = i;
            break;
        }
        case H_KEY: {
            if (str[i] == ':') {
                key_end = i;
                if (key_end - key_start <= 0)
                    return PARSE_HEADER_ERROR;
                hState = H_COLON;
            } else if (str[i] == '\n' || str[i] == '\r')
                return PARSE_HEADER_ERROR;
            break;
        }
        case H_COLON: {
            if (str[i] == ' ')
                hState = H_SPACES_AFTER_COLON;
            else
                return PARSE_HEADER_ERROR;
            break;
        }
        case H_SPACES_AFTER_COLON: {
            hState = H_VALUE;
            value_start = i;
            break;
        }
        case H_VALUE: {
            if (str[i] == '\r') {
                hState = H_CR;
                value_end = i;
                if (value_end - value_start <= 0)
                    return PARSE_HEADER_ERROR;
            } else if (i - value_start > 255)
                return PARSE_HEADER_ERROR;
            break;
        }
        case H_CR: {
            if (str[i] == '\n') {
                hState = H_LF;
                string key(str.begin() + key_start, str.begin() + key_end);
                string value(str.begin() + value_start, str.begin() + value_end);
                headers[key] = value;
                now_read_line_begin = i;
            } else
                return PARSE_HEADER_ERROR;
            break;
        }
        case H_LF: {
            if (str[i] == '\r') {
                hState = H_END_CR;
            } else {
                key_start = i;
                hState = H_KEY;
            }
            break;
        }
        case H_END_CR: {
            if (str[i] == '\n') {
                hState = H_END_LF;
            } else
                return PARSE_HEADER_ERROR;
            break;
        }
        case H_END_LF: {
            notFinish = false;
            key_start = i;
            now_read_line_begin = i;
            break;
        }
        }
    }
    if (hState == H_END_LF) {
        str = str.substr(i);
        return PARSE_HEADER_SUCCESS;
    }
    str = str.substr(now_read_line_begin);

    return PARSE_HEADER_AGAIN;
}

AnalysisState HttpData::analysisRequest()
{
    if (method == METHOD_POST) {
        
    } else if (method == METHOD_GET || method == METHOD_HEAD) {
        string header;
        header += "HTTP/1.1 200 OK\r\n";

        // 设置 Keep-Alive 选项
        if (headers.find("Connection") != headers.end()
            && (headers["Connection"] == "Keep-Alive"
            || headers["Connection"] == "keep-alive")) {
            
            keepAlive = true;
            header += string("Connection: Keep-Alive\r\n") + "Keep-Alive: timeout=" +
                        to_string(DEFAULT_KEEP_ALIVE_TIME) + "\r\n";
        }

        // 设置 MIME 类型
        int dot_pos = fileName.find('.');
        string filetype;
        if (dot_pos < 0)
            filetype = MimeType::getMime("default");
        else
            filetype = MimeType::getMime(fileName.substr(dot_pos));

        // echo test
        if (fileName == "hello") {
            outBuffer =
                "HTTP/1.1 200 OK\r\nContent-type: text/plain\r\n\r\nHello World";
            return ANALYSIS_SUCCESS;
        }

        struct stat sbuf;
        if (stat(fileName.c_str(), &sbuf) < 0) {
            header.clear();
            handleError(fd, 404, "Not Found!");

            return ANALYSIS_ERROR;
        }
        header += "Content-Type: " + filetype + "\r\n";
        header += "Content-Length: " + to_string(sbuf.st_size) + "\r\n";
        header += "Server: LinYa's Web Server\r\n";
        header += "\r\n";

        outBuffer += header;

        if (method == METHOD_HEAD)
            return ANALYSIS_SUCCESS;

        // 读取文件
        int src_fd = open(fileName.c_str(), O_RDONLY, 0);
        if (src_fd < 0) {
            outBuffer.clear();
            handleError(fd, 404, "Not Found!");

            return ANALYSIS_ERROR;
        }
        void *mmapRet = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
        close(src_fd);
        if (mmapRet == (void *)-1) {
            // munmap(mmapRet, sbuf.st_size); 
            outBuffer.clear();
            handleError(fd, 404, "Not Found!");
            return ANALYSIS_ERROR;
        }
        char *src_addr = static_cast<char *>(mmapRet);
        outBuffer += string(src_addr, src_addr + sbuf.st_size);
        
        munmap(mmapRet, sbuf.st_size);
        return ANALYSIS_SUCCESS;
    }
    return ANALYSIS_ERROR;
}

void HttpData::handleError(int fd, int err_num, string short_msg)
{
    short_msg = " " + short_msg;
    char send_buff[4096];
    string body_buff, header_buff;
    body_buff += "<html><title>哎~出错了</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += to_string(err_num) + short_msg;
    body_buff += "<hr><em> LinYa's Web Server</em>\n</body></html>";

    header_buff += "HTTP/1.1 " + to_string(err_num) + short_msg + "\r\n";
    header_buff += "Content-Type: text/html\r\n";
    header_buff += "Connection: Close\r\n";
    header_buff += "Content-Length: " + to_string(body_buff.size()) + "\r\n";
    header_buff += "Server: LinYa's Web Server\r\n";
    ;
    header_buff += "\r\n";
    // 错误处理不考虑writen不完的情况
    sprintf(send_buff, "%s", header_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
    sprintf(send_buff, "%s", body_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
}

void HttpData::handleClose()
{
    connectionState = H_DISCONNECTED;
    shared_ptr<HttpData> guard(shared_from_this());
    loop->removeFromPoller(channel);
}

void HttpData::newEvent()
{
    channel->setEvents(DEFAULT_EVENT);
    loop->addToPoller(channel, DEFAULT_EXPIRED_TIME);
}
