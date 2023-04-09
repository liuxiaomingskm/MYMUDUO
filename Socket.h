#pragma once

#include "noncopyable.h"

class InetAddress;

//封装socket fd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
        {}


    ~Socket();

    int fd() const {return sockfd_;}
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);

    void shutdownWrite();
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on); // 设置地址重用 
    void setReusePort(bool on); // 设置端口重用 
    void setKeepAlive(bool on); // 设置保活 

private:
    const int sockfd_;

};