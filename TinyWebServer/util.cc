#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>

constexpr int MAX_BUF   = 4096;
constexpr int QUEUE_NUM = 2048;

ssize_t readn(int fd, void *buf, size_t n)
{
    size_t  nleft    = n;
    ssize_t nread    = 0;
    ssize_t read_sum = 0;
    char *ptr = (char *)buf;

    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;
            else if (errno == EAGAIN)
                return read_sum;
            else
                return -1;
        }
        else if (nread == 0)
            break;
        
        read_sum += nread;
        nleft -= nread;
        ptr += nread;
    }

    return read_sum;
}

ssize_t readn(int fd, std::string &in_buf, bool &read_zero)
{
    ssize_t nread    = 0;
    ssize_t read_sum = 0;

    while (true) {
        char buf[MAX_BUF];
        if ((nread == read(fd, buf, MAX_BUF)) < 0) {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
                return read_sum;
            else
                return -1;
        }
        else if (nread == 0) {
            read_zero = true;
            break;
        }

        read_sum += nread;
        in_buf += std::string(buf, buf + nread);
    }

    return read_sum;
}

ssize_t readn(int fd, std::string &in_buf)
{
    ssize_t nread    = 0;
    ssize_t read_sum = 0;

    while (true) {
        char buf[MAX_BUF];
        if ((nread = read(fd, buf, MAX_BUF)) < 0) {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
                return read_sum;
            else
                return -1;
        }
        else if (nread == 0)
            break;
        
        read_sum += nread;
        in_buf += std::string(buf, buf + nread);
    }

    return read_sum;
}

ssize_t writen(int fd, void *buf, size_t n)
{
    size_t  nleft     = n;
    ssize_t nwritten   = 0;
    ssize_t write_sum = 0;
    char *ptr = (char *)buf;

    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0) {
                if (errno == EINTR) {
                    nwritten = 0;
                    continue;
                }
                else if (errno == EAGAIN)
                    return write_sum;
                else
                    return -1;
            }
        }

        write_sum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }

    return write_sum;
}

ssize_t writen(int fd, std::string &sbuf)
{
    size_t  nleft     = sbuf.length();
    ssize_t nwritten  = 0;
    ssize_t write_sum = 0;
    const char *ptr = sbuf.c_str();

    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0) {
                if (errno == EINTR) {
                    nwritten = 0;
                    continue;
                }
                else if (errno == EAGAIN)
                    break;
                else
                    -1;
            }
        }

        write_sum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }

    // clear the sent data in sbuf
    if (write_sum == static_cast<ssize_t>(sbuf.size()))
        sbuf.clear();
    else
        sbuf = sbuf.substr(write_sum);

    return write_sum;
}

void handle_for_sigpipe()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags |= SA_RESTART;        // add restart system call flag

    int ret =  sigaction(SIGPIPE, &sa, NULL);
    assert(ret != -1);
}

void set_nonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL, 0);
    int new_option = old_option | O_NONBLOCK;

    int ret = fcntl(fd, F_SETFL, new_option);
    assert(ret != -1);
}

void set_socket_nodelay(int fd)
{
    int enable = 1;
    int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable,
                         sizeof(enable));
    assert(ret != -1);
}

void set_socket_nolinger(int fd)
{
    struct linger lin;
    lin.l_onoff = 1;
    lin.l_linger = 30;

    // enable linger time, 30s
    int ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char *)&lin,
                         sizeof(lin));
    assert(ret != -1);
}

void shutdown_wr(int fd)
{
    shutdown(fd, SHUT_WR);
}

int socket_bind_listen(int port)
{
    if (port < 0 | port > 65535)
        return -1;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listen_fd != -1);

    // eliminate "address already in use" error
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) != 0) {
        close(listen_fd);
        return -1;
    }

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t)port);
    if (bind(listen_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) != 0) {
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, QUEUE_NUM) != 0) {
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}