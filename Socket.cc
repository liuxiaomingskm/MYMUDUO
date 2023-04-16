#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h> // only for strcasecmp, strncasecmp, bzero
#include <netinet/tcp.h> // for TCP_NODELAY and all other tCP socket options
#include <sys/socket.h>

Socket::~Socket() {
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    if ( 0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in))) {
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
    }
}

void Socket::listen()
{
    if ( 0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr)
{
    /**
     * 1. accept函数的参数不合法
     * 2. 对返回的confd没有设置非阻塞
     * 3. Reactor模型 one loop per thread
     * poller + non-blocking IO
    */
   sockaddr_in addr;
   socklen_t len = sizeof(addr);
   bzero(&addr, len);
   int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC); // 链接之后 会返回sockaddr_in，需要把这个参数传回peeraddr中，方便后面调用
   if (connfd >= 0)
   {
    peeraddr -> setSockAddr(addr); // 客户端端口号和地址保存在peeraddr中，同时返回connfd
   }
   return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownWrite sockfd: %d fail \n", sockfd_);
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1: 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1: 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);    
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1: 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}

/**
 * TCP_NODELAY (setTcpNoDelay() method): This option is used to control the Nagle's algorithm for a TCP socket. When this option is enabled (set to 1), 
 * the algorithm is disabled, and small packets are sent immediately without waiting for the buffer to fill up or an 
 * acknowledgment for previously sent data. This can reduce latency for some applications, like interactive or real-time applications, 
 * at the cost of potentially increased network congestion.

SO_REUSEADDR (setReuseAddr() method): This option allows multiple sockets to bind to the same address and port, 
as long as they all set this option before binding. This is useful in situations where a server needs to be restarted and 
the previous socket is still in the TIME_WAIT state. Enabling this option allows the server to bind to the same address and port 
without waiting for the previous socket to fully close, reducing downtime.

SO_REUSEPORT (setReusePort() method): This option allows multiple sockets to bind to the same address and port, 
with the operating system distributing incoming connections among the bound sockets. This can be helpful for load balancing a
nd enables multiple processes or threads to share the same port, improving performance.

SO_KEEPALIVE (setKeepAlive() method): This option enables the periodic transmission of keep-alive probes for a TCP connection 
when there is no data exchanged for a specified amount of time. If the remote endpoint does not respond to the keep-alive probes, t
he connection is considered broken and will be closed. Enabling this option can help detect broken connections and free up resources in a more timely manner.
*/