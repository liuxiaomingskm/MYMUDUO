#include "EventLoopThread.h"
#include "EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallBack &cb,
        const std::string &name)
        : loop_(nullptr)
        , exiting_(false)
        , thread_(std::bind(&EventLoopThread::threadFunc, this),name)  
        , mutex_() // 默认构造函数，不需要构造一个mutex然后传进去
        , cond_() // 默认构造函数
        , callback_(cb)
        {

        }


EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_ -> quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() 
{
    thread_.start(); //启动底层线程

    EventLoop * loop = nullptr;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        /**
         * 这里必须设置一个while循环 因为C++允许spurious wakeup.
         * Spurious wakeups are when a thread waiting on a condition variable is awakened without the condition variable being explicitly signaled or notified.
         * 所以为了保证loop_不为空，必须在loop_不为空的时候才能退出while循环
        */
        while(loop_ == nullptr) 
        {
            cond_.wait(lock);
        }
        loop = loop_;//将新线程创建的loop返回
    }
    return loop;
}

//下面这个方法是在单独的新线程里面运行的 //这段代码略有不懂 需要结合视频观看
void EventLoopThread::threadFunc()
{
    EventLoop loop; //创建一个独立的eventloop，和上面的线程是一一对应的，one loop per thread , loop只在刚创建的线程里被创建，所以one loop per thread

    if(callback_)
    {
        callback_(&loop);

    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop(); // EventLoop loop -> Poller.poll // 一直处于阻塞状态
    std::unique_lock<std::mutex> lock(mutex_); // 当到达这里的时候说明当前loop要关闭了 所以将loop赋值nullptr
    loop_ = nullptr;
}