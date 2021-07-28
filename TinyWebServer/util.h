#pragma once

#include <cstdlib>
#include <string>

// try best to read n bytes data
ssize_t readn(int fd, void *buf, size_t n);

// try best to read data, and indicate if connection ended(read_zero)
ssize_t readn(int fd, std::string &inBuf, bool &read_zero);

// try best to read data
ssize_t readn(int fd, std::string &inBuf);

// try best to write n bytes data
ssize_t writen(int fd, void *buf, size_t n);

// try best to write sbuf, and modify the sbuf to the unsent data
ssize_t writen(int fd, std::string &sbuf);



void handle_for_sigpipe();

void set_nonblocking(int fd);

void set_socket_nodelay(int fd);

void set_socket_nolinger(int fd);

void shutdown_wr(int fd);

int  socket_bind_listen(int port);