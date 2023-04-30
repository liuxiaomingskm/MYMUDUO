#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户链接，通过accept函数拿到connfd
 * => TcpConnection 设置回调 -> Channel -> Poller -> Channel的回调操作
*/
// tcp连接代表已经建立的一条通信链路，封装了所有的和connection有关的内容 比如channel等等 tcp的一部分数据打包成channel扔给poller
// enable_shared_from_this 用来解决单个object有多个shared_ptr指向的问题,比如当前object被两个shared_ptr指向，每个shared_ptr单独计算引用数量 且都为1,
// 当两个shared_ptr都析构时，当前object就会被析构两次，从而触发错误
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                    const std::string &name,
                    int sockfd,
                    const InetAddress &localAddr,
                    const InetAddress &peerAddr);


    ~TcpConnection();

    EventLoop* getLoop() const {return loop_;}
    const std::string& name() {return name_;}
    const InetAddress& localAddress() {return localAddr_;}
    const InetAddress& peerAddresss() {return peerAddr_;}

    bool connected() const {return state_ == kConnected;}

    //发送数据
    void send(const std::string &buff);

    void send(const void *message, size_t len);

    //关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    {connectionCallback_ = cb;}
 
    void setMessageCallback(const MessageCallback& cb)
    {messageCallback_ = cb;}

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    {writeCompleteCallback_ = cb;}

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    {highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark;}

    void setCloseCallback(const CloseCallback& cb)
    {closeCallback_ = cb;}

    //链接建立
    void connectionEstablished();
    //链接销毁
    void connectionDestroyed();
private:
    enum StateE {kConnecting, kDisconnected,kConnected, kDisconnecting};
    void setState (StateE state) {state_ = state;}

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; //这里不是base loop,因为TcpConnection是在subloop里面管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    //这里和Acceptor类似，Acceptor => mainLoop TcpConnection => subLoop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; // 有新连接时的回调
    MessageCallback messageCallback_; // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_; //接收数据的缓冲区
    Buffer outputBuffer_; //发送数据的缓冲区
};