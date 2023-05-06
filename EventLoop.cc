#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"


#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>


//防止一个线程创建多个EventLoop, eventLoop是全局变量，当不为null的时候 就不再创建
// __thread相当于threadLocal，如果没有这个标志 相当于所有线程共享这个变量，如果加上__thread,说明每个线程都有自己的
//一个副本
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller的 IO复用接口的超时时间
const int kPollTimeMs = 10000;

//创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createFd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("eventfd error: %d \n", errno); //自带exit
    }
    return evtfd;
}

// 每个eventloop都有一个单独的epollfd，用来上树和下树channel
EventLoop:: EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()), //tid是inline方法
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createFd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(nullptr){
        LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);

        if (t_loopInThisThread){
            LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
        }
        else {
            t_loopInThisThread = this;
        }

        //设置wakeupfd的事件类型以及发生事件后的回调 ??
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
        //每一个eventloop都将监听wakeupChannel的EPOLLIN读事件
        wakeupChannel_->enableReading();
      }

      EventLoop::~EventLoop(){
        wakeupChannel_ -> disableAll();
        wakeupChannel_ -> remove();
        ::close(wakeupFd_);
        t_loopInThisThread = nullptr;
      }

      //开启事件循环
      void EventLoop::loop()
      {
        looping_ = true;
        quit_ = false;

        LOG_INFO("EventLoop %p start looping \n", this);

        while(!quit_){
            activeChannels_.clear();
            //监听两类fd 一种是client的fd，一种是wakeup fd
            pollReturnTime_ = poller_ -> poll(kPollTimeMs, &activeChannels_);

            for (Channel *channel : activeChannels_)
            {
                //Poller监听哪些channel发生事件了，然后上报给EventLoop,通知channel处理相应事件
                channel -> handleEvent(pollReturnTime_);
            }
            //执行当前EventLoop事件循环需要处理的回调操作
            /**
            * IO线程 mainLoop accept fd =>> channel subloop
            * mainLoop 事先注册一个回调cb（需要subloop来执行） wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作,
            * 这里的callback都放在vector<Functor>中
            */
            doPendingFunctors();
        }

        LOG_INFO("EventLoop %p stop looping. \n", this);
        looping_ = false;
      }

      //
// 退出事件循环  1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit
/**
 *              mainLoop
 * 
 *                                             no ==================== 生产者-消费者的线程安全的队列
 * 
 *  subLoop1     subLoop2     subLoop3
 */ 
      void EventLoop::quit()
      {
        quit_=true;
        //如果是在其他线程中，调用的quit，比如在一个subloop（worker）中，调用了mainLoop(IO)的quit
        //如果此时mainloop正在epoll_wait,block中，那么需要wakeup,如果正在处理其他事情，事情处理后就会直接跳出while循环，
        // wakeup不会影响退出
        if(!isInLoopThread())
        {
            wakeup();
        }
      }

      //在当前loop中执行cb
      void EventLoop::runInLoop(Functor cb) {
        if (isInLoopThread()) {
            //在当前的loop线程中执行cb
            cb();
        }
        else {
            //在非当前loop线程中执行cb，就需要唤醒loop所在线程，执行cb
            queueInLoop(cb);
        }
      }

      //把cb放入队列中，唤醒loop所在的线程，执行cb
      void EventLoop::queueInLoop(Functor cb)
      {
        {
            //因为可能多个线程需要往vector里面加callback 所以需要锁来控制
            std::unique_lock<std::mutex> lock(mutex_);
            pendingFunctors_.emplace_back(cb);
        }

        //唤醒相应的，需要执行上面回调操作的的loop的线程
        // || callPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
        if (!isInLoopThread() || callingPendingFunctors_)
        {
            wakeup(); //唤醒loop所在线程
        }
      }

      void EventLoop::handleRead() 
      {
        uint64_t one = 1;
        ssize_t n = read(wakeupFd_, &one, sizeof one);
        if (n != sizeof one) 
        {
            LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n); //这个channel只是用来唤醒eventloop 实际内容无所谓
        }
      }

      //用来唤醒loop所在的线程，向wakeupfd_写一个数据，wakeupChannel就发生读事件，当前loop线程就被唤醒了
      void EventLoop::wakeup()
      {
        uint64_t one = 1;
        ssize_t n = write(wakeupFd_, &one, sizeof one);
        if (n != sizeof one)
        {
            LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
        }
      }

      //EventLoop的方法 -> Poller的方法
      void EventLoop::updateChannel(Channel * channel)
      {
        poller_ -> updateChannel(channel);
      }

        void EventLoop::removeChannel(Channel * channel)
      {
        poller_ -> removeChannel(channel);
      }

        bool EventLoop::hasChannel(Channel * channel)
      {
        return poller_ -> hasChannel(channel);
      }

      //执行回调
      void EventLoop::doPendingFunctors() //执行回调
      {
        std::vector<Functor> functors;
        callingPendingFunctors_ = true;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            functors.swap(pendingFunctors_);
        }
        for (const Functor &functor : functors)
        {
            functor(); //执行当前loop需要执行的回调操作
        }
        callingPendingFunctors_ = false;
      }



/**
 * Difference meaning of block and non-block in epoll_wait and eventFd
The epoll_wait function is used to wait for events on file descriptors monitored by an epoll instance. It has a timeout parameter that determines how long the function should block if there are no events available:

If the timeout is set to -1, epoll_wait will block indefinitely until an event occurs.
If the timeout is set to 0, epoll_wait will return immediately, even if no events are available (i.e., it's non-blocking).
If the timeout is set to a positive value, epoll_wait will block for up to that number of milliseconds waiting for an event.
The blocking or non-blocking mode of a file descriptor affects its read and write operations, not the behavior of epoll_wait. When you add a file descriptor to an epoll instance, the epoll instance will monitor the file descriptor for events, such as data being available to read or the descriptor being ready to accept a write operation.

If a file descriptor is set to non-blocking mode and epoll_wait returns an event for that descriptor, you can perform read or write operations on the descriptor without blocking. However, if the file descriptor is in blocking mode and there's not enough data to read or space to write, the read or write operations will block until the conditions are met, even if epoll_wait itself didn't block.

In summary, the blocking mode of a file descriptor affects its read and write operations but not the behavior of epoll_wait. The behavior of epoll_wait is determined by its timeout parameter.


so set file descriptor in non-block mode, means if it try to read data, it will immediately read all data and return. If there is no data avaiable, it will still return?
Yes, that's correct. When a file descriptor is set to non-blocking mode, read operations behave as follows:

If there is data available to read, the read operation will immediately read as much data as possible (up to the specified buffer size) and then return the number of bytes read.
If there is no data available to read at the moment, the read operation will not block and wait for data. Instead, it will immediately return -1, and the errno variable will be set to EAGAIN or EWOULDBLOCK, indicating that the operation would have blocked if the file descriptor were in blocking mode.
In non-blocking mode, the program can continue executing other tasks without waiting for data to become available on the file descriptor. This can help improve the responsiveness and performance of event-driven or asynchronous applications.
*/
