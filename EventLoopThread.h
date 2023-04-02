#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable> //条件变量
#include <string>

class EventLoop;


class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallBack = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallBack &cb =  ThreadInitCallBack(),  // 默认是空回调函数，如果user不提供的话，该函数主要用于初始化时，用户想要运行的函数
        const std::string &name = std::string());
    
    ~EventLoopThread();

    EventLoop* startLoop();
private:
    void threadFunc(); //创建loop

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallBack callback_;
};