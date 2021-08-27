#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <functional>

#include "Server.h"
#include "util.h"
#include "base/Logger.h"

Server::Server(EventLoop *_loop, int _threadNum, int _port)
    : loop(_loop),
      threadNum(_threadNum),
      port(_port),
      started(false),
      eltp (make_unique<EventLoopThreadPool>(_loop, _threadNum)),
      acceptChannel(make_shared<Channel>(_loop)),
      listenfd(socket_bind_listen(_port))
{
    acceptChannel->setFd(port);
    handle_for_sigpipe();
    set_nonblocking(listenfd);
}

void Server::start()
{
    eltp->start();
    acceptChannel->setEvents(EPOLLIN | EPOLLET);
    acceptChannel->setReadHandler(bind(&Server::handleNewConn, this));
    acceptChannel->setConnHandler(bind(&Server::handleThisConn, this));
    loop->addToPoller(acceptChannel, 0);
    started = true;
}

void Server::handleNewConn()
{
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    memset(&client_addr, 0, client_addr_len);
    int accept_fd = 0;
    while ((accept_fd = accept(listenfd, (sockaddr *)&client_addr,
                               &client_addr_len)) > 0) {
        EventLoop *loop = eltp->getNextLoop();
        LOG << "New Connection from " << inet_ntoa(client_addr.sin_addr) << ":"
            << ntohs(client_addr.sin_port);
        if (accept_fd > MAXFDS) {
            close(accept_fd);
            continue;
        }

        set_nonblocking(accept_fd);
        set_socket_nodelay(accept_fd);

        shared_ptr<HttpData> req_info = make_shared<HttpData>(loop, accept_fd);
        req_info->getChannel()->setHolder(req_info);
        loop->queueInLoop(std::bind(&HttpData::newEvent, req_info));
    }
    acceptChannel->setEvents(EPOLLIN | EPOLLET);
}