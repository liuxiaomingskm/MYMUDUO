#include  "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0); //静态成员变量需要在类外单独定义

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false),
        joined_(false),
        tid_(0),
        func_(std::move(func)),
        name_(name)
        {
            setDefaultName();
        }
Thread::~Thread() {
    if (started_ && !joined_) { //  如果线程已经启动，但是没有等待，总共两种状态joined_和!joined_ 即detach，被joined的线程结束后有调用join的线程回收资源，detach自动回收
        thread_ -> detach(); //thread类提供的设置分离线程的方法
    }
}

void Thread::start() //一个thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false , 0);

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){ //lambda表达式 以引用的方式传递外部所有成员变量 
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem); // 释放信号量，表示获取到了tid值 信号量的值加1
        // 开启一个新线程，专门执行该线程函数
        func_(); 
    }));

    // 这里必须等待获取上面新创建的线程的tid值
    sem_wait(&sem); //此时已经有了tid值
}
void Thread::join() {
    joined_ = true;
    thread_ -> join();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if (name_.empty()){
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d",num);
        name_ = buf;
    }
}