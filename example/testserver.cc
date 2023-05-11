#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop,
                const InetAddress &addr,
                const std::string &nameArg)
                :server_(loop, addr, nameArg)
                , loop_(loop)
    {
        //注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
        );

        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );

        //设置合适的loop线程数量 loopthread
        server_.setThreadNum(5);
    }    

    void start()
    {
        server_.start();
    }

private:
    // 连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn-> connected())
        {
            LOG_INFO("Connection UP: %s", conn->peerAddress().toIpPort().c_str());
        }
        else {
            LOG_INFO("Connection DOWN: %s", conn->peerAddress().toIpPort().c_str());
        } 
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg(buf->retrieveAllAsString());            
        

        conn->send(msg);
        conn->shutdown(); //关闭写端 epollhup -> closeCallback_;
    }

// 没有深层封装，用户仍需要创建loop
    EventLoop *loop_;
    TcpServer server_;
};
 
int main  ()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "EchoServer-01"); // acceptor non-blocking listenfd create build
    server.start(); // listen loopthread lsitenfd=> acceptChannel -> mainloop socket开始监听listen
    loop.loop(); // 启动mainloop的底层poller epoll把listenfd加入到epoll中，调用epoll wait监听是否有新的客户端链接
    


    return 0;
}
