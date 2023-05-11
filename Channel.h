#pragma once

#include "noncopyable.h"
#include "Timestamp.h" // 只有简单声明指针时 才使用前置声明，一旦涉及到具体实例，必须引进相应头文件
#include <functional>
#include <memory>

#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>
#include <netinet/in.h>
#include <fcntl.h>
class EventLoop;


/*
    理清楚EventLoop,channel, poller之间的关系 对应于reactor模型上的Demultiplex？存疑
    channel理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN, EPOLLOUT事件，
    还绑定了poller返回的具体事件
*/
class Channel : noncopyable
{
    public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件的。
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb) {readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);} 
    void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb);} 
    void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb);} 
    //防止当channel被手动remove掉，channel还在执行回调操作，eventloop会remove掉channel
    void tie(const std::shared_ptr<void>&);

    int fd() const {return fd_;}
    int events() const {return events_;}
    void set_revents (int revt) {revents_ = revt;} // used by pollers  channel没办法监听收到的事件，poll可以，所以需要接口来设置收到的事件

    //设置fd感兴趣的事件状态
    void enableReading() {events_ |= kReadEvent; update();}
    void disableReading() {events_ &= ~kReadEvent; update();}
    void enableWriting() {events_ |= kWriteEvent; update();}
    void disableWriting() {events_ &= ~kWriteEvent; update();}
    void disableAll() {events_ = kNoneEvent; update();}

    //返回fd当前的事件状态
    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isWriting() const {return events_ & kWriteEvent;}
    bool isReading() const {return events_ & kReadEvent;}

    int index() {return index_;}
    void set_index(int idx) {index_ = idx;}

    //one loop per thread
    EventLoop* ownerLoop() {return loop_;}
    void remove();

    private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent; //表示当前fd的一个状态，对读事件感兴趣，写事件感兴趣，或者都不感兴趣
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; //事件循环
    const int fd_; // fd, poller监听的对象
    int events_; //注册fd感兴趣的事件
    int revents_; //poller返回的具体事件
    int index_; 

/**
 * using a std::weak_ptr for the tie_ member variable helps manage object lifetimes, avoid circular dependencies, 
 * and ensure that the "tied" object is not kept alive longer than necessary.主要是为了防止循环引用
*/
    std::weak_ptr<void> tie_;
    
    bool tied_;

// 因为channel通道里面能够获知fd最终发生的具体事件revents, 所以他负责调用具体事件的回调
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
