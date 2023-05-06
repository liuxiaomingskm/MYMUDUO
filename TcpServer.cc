#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include "strings.h"
#include "functional"

static EventLoop* checkLoopNotNull(EventLoop * loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer :: TcpServer(EventLoop *loop,
                        const InetAddress &listenAddr,
                        const std::string &nameArg,
                        Option option)
                        : loop_(checkLoopNotNull(loop))
                        , ipPort_(listenAddr.toIpPort())
                        , name_(nameArg)
                        , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
                        , threadPool_(new EventLoopThreadPool(loop, name_))
                        , connectionCallback_() // empty function object 如果用户没有设置回调，就是空函数对象，没有任何操作
                        , messageCallback_()
                        , nextConnId_(1)
                        , started_(0)
{
    // 当有用户链接时，会执行TcpServer::newConnection回调
    acceptor_-> setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, 
        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item: connections_)
    {
        //这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象的资源
        //整个程序每个connection都只有一个shared_ptr指向，而这个指针就是map里的value，创建新conn，reference count + 1,
        // 调用reset，reference count - 1,最后局部变量conn跳出scope时，销毁conn自动释放资源
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        //销毁链接
        conn -> getLoop() -> runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_ -> setThreadNum(numThreads);
}

//开启服务器监听 loop.loop()
void TcpServer::start()
{
    if (started_++ == 0) // 防止TcpServer对象被start多次,只有第一次才成功启动 后面++就不会执行了
    {
        threadPool_ -> start(threadInitCallback_); //启动底层的loop线程池
        loop_ -> runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

//有一个新的客户端的链接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    //轮询算法，选择一个subloop， 来管理channel
    EventLoop* ioLoop = threadPool_ ->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_; //只有在mainloop中执行，所以不需要加锁
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new Connection [%s]  from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen)<0) 
    {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);

    // 根据链接成功的sockfd，创建TcpConnection对象
    TcpConnectionPtr conn(new TcpConnection(
                            ioLoop,
                            connName,
                            sockfd, // socket channel 通过sockfd可以创建channel和Socket对象
                            localAddr,
                            peerAddr));

    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer => TcpConnection => Channel => EventLoop
    conn -> setConnectionCallback(connectionCallback_);
    conn -> setMessageCallback(messageCallback_);
    conn -> setWriteCompleteCallback(writeCompleteCallback_);

    // 设置链接关闭的回调 conn -> shutDown()
    conn -> setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    //直接调用TcpConnection::connectEstablished()，这个函数会在TcpConnection所属的loop中执行
    ioLoop -> runInLoop(std::bind(&TcpConnection::connectEstablished, conn));

}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_ -> runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s \n",
        name_.c_str(), conn -> name().c_str());

    connections_.erase(conn -> name());
    EventLoop *ioLoop = conn -> getLoop();
    //connection设置了coonectDestoryed和connectEstablished的回调，这里通过TcpServer::removeConnectionInLoop()来调用
    ioLoop -> queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}