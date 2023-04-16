#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"


#include  <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0)
      {

      }

EventLoopThreadPool::~EventLoopThreadPool()
{
    //不需要手动释放 因为loop是栈变量，会自动释放
}

void EventLoopThreadPool::start(const ThreadInitCallBack &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; i++) {
        
        char buf[name_.size() + 32]; // ?
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t)); // unique_ptr 不能拷贝，只能移动, 转化t指针为unique_ptr指针
        loops_.push_back(t -> startLoop()); // 底层创建线程，绑定一个新的EventLoop，并返回该loop的地址
    }

    // 整个服务端只有一个线程，运行着baseloop
    if(numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

//如果在多线程中，baseLoop默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseLoop_;

    if  (!loops_.empty()) {
        loop = loops_[next_];
        next_++;
        if (next_ >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    if (loops_.empty()){
        return std::vector<EventLoop*>(1, baseLoop_); // vector构造函数 std::vector<T> v(size_t n, const T& value); 用value初始化n个元素
    }
    else {
        return loops_;
    }
}
