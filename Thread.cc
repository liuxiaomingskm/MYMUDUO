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
    /**
     * 析够函数调用前提
     * 1.线程已经启动
     * 2.如果设置joined 说明调用了thread ->joined，那么目标线程即thread会继续执行到结束，然后返回状态给调用这个方法的线程，由操作系统或者调用的线程进行资源的回收
     * 3.如果没有设置joined 说明没有任何线程调用目标线程的joined方法，那么存在可能性目标线程是最后执行完成的，所以设置成detached 这样结束后就能自己回收资源了
     * 
    */ 
        if (started_ && !joined_) { 
        thread_ -> detach(); //thread类提供的设置分离线程的方法
    }
}

void Thread::start() //一个thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false , 0); // false意思是只在同一个进程的线程之间共享，0为初始值，意思之没有任何线程能够占用这个semphore

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