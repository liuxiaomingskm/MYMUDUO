#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd_)
    : loop_(loop), fd_(fd_), events_(0), revents_(0), index_(-1), tie_d(false) // index是当前channel的状态 是否已经添加到epoll中
{

}

Channel::~Channel()
{
}

// channel的tie方法什么时候调用过？ 为什么要tie？ 因为这里的obj其实是TcpConnection的shared_from_this() 所有的回调函数 其实都是tecpConnection的成员函数，
// 所以为了防止调用的时候TcpConnection已经析构了，所以需要绑定一个shared_ptr来延长TcpConnection的生命周期
void Channel::tie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tie_d = true;
}

/**
 * 当改变channel所表示fd的events事件后，update负责在poller里面更改fd响应的时间
 * EventLoop -> ChannelList Poller channel无法调用poller方法 只能通过eventLoop实现
*/
void Channel::update() {
    //通过channel所属的eventLoop，调用poller的相应方法来注册fd的events事件
    // add code ...
    loop_ -> updateChannel(this);
}

//在channel所属的eventLoop中，把当前的channel删除掉
void Channel::remove(){
    // add code
   loop_ -> removeChannel(this);
}

//fd得到poller通知以后，处理事件
void Channel::handleEvent(Timestamp receiveTime) {
    if(tie_d) {
        std::shared_ptr<void> guard = tie_.lock(); //提升weak pinter
        if (guard){
            handleEventWithGuard(receiveTime);
        }
    }
    else {
        handleEventWithGuard(receiveTime);
    }
}

//根据poller通知的channel发生的具体事件，有channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
   // LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_) // 调用的是TcpConnection::handleClose方法
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
