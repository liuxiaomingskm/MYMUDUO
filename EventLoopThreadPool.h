#pragma once

#include "noncopyable.h"
#include "EventLoopThread.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopthread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallBack = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);

    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) {numThreads_ = numThreads;}

    void start(const ThreadInitCallBack &cb = ThreadInitCallBack());

    //如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const {return started_;}
    const std::string& name() const {return name_;}

    private:

    EventLoop *baseLoop_;  // EventLoop loop 用户创建的一开始的loop 如果numThreads_ = 0 则baseLoop_就是subloop 同时负责accept和IO
    std::string name_;
    bool started_;
    int numThreads_;
    int next_; //下一个要使用的线程的序号
    std::vector<std::unique_ptr<EventLoopThread>> threads_; //线程池
    std::vector<EventLoop*> loops_; //保存所有的loop
};