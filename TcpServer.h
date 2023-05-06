#pragma once

/**
 * 用户使用muduo编写服务器程序
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional> // std::bind
#include <string>
#include <memory> // std::shared_ptr
#include <atomic>
#include <unordered_map>


//对外的服务器编程使用的类
class TcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void (EventLoop*)>;

    enum Option {
        kNoReusePort,
        kReusePort
    };

    TcpServer(EventLoop * loop,
                const InetAddress &listenAddr,
                const std::string &nameArg,
                Option option = kNoReusePort);
    ~TcpServer();

// tcpServer能够设置不同的回调函数 并且传入TcpConnection，但是对所有的TcpConnection都是一样的回调函数 如果需要设置不同的回调函数
// 可以在回调函数内进行判断
    void setThreadInitCallback (const ThreadInitCallback &cb) {threadInitCallback_ = cb;}
    void setConnectionCallback (const ConnectionCallback &cb) {connectionCallback_ = cb;}
    void setMessageCallback (const MessageCallback &cb) {messageCallback_ = cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {writeCompleteCallback_ = cb;}

    //设置底层subloop的个数
    void setThreadNum(int numThreads);

    //开启服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop * loop_; //base loop 用户定义的loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; //运行在mainLoop，任务就是监听新链接时间 avoid revealing Acceptor 通过指针的前置声明，避免暴露acceptor类
    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_; //有新链接时的回调
    MessageCallback messageCallback_; //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_; //loop线程初始化的回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; // 保存所有链接
};