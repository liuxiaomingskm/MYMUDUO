#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h> // pid_t
#include <sys/socket.h> // socket
#include <errno.h> // errno
#include <unistd.h> // close

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0); 
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d, listen socket create error:%d \n", __FILE__, __FUNCTION__, __LINE__,errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop * loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
    acceptSocket_(createNonblocking()), // 创建一个非阻塞的socket
    acceptChannel_(loop, acceptSocket_.fd()), 
    listenning_(false)
{
    acceptSocket_.setReuseAddr(true);;
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); // bind
    // TcpServer::start() -> Acceptor.listen() -> Channel.enableReading() -> Poller.updateChannel() -> epoll_ctl()
    // baseLoop -> acceptChannel_(listenfd) ->
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading(); //acceptChannel -> Poller.updateChannel() -> epoll_ctl()
}
    // listenfd有事件发生了，就是有新用户链接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr); //connfd 新链接fd，peerAddr客户端地址和端口 轮询找到subLoop，唤醒，分发当前的新客户端的channel
        }
        else {
            ::close(connfd); // 如果没有回调函数，就直接关闭
        }
    }
    else {
        LOG_ERROR("%s:%s:%d, accept error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) // EMFILE:: two many open files
        {
            LOG_ERROR("%s:%s:%d, sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }


}
