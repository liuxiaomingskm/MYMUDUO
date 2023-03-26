#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

//channel添加到poller中
const int kNew = -1; // channel的成员index_ = -1
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EpollPoller :: EpollPoller(EventLoop* loop) 
    : Poller(loop) //调用基类构造函数来初始化基类成员
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize) //vector <epoll_event> //初始化vector大小
{
    if (epollfd_ < 0)
    { 
        LOG_FATAL("epoll_create error : %d \n", errno);
    }
}

EpollPoller::~EpollPoller() {
    ::close(epollfd_);
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    //实际应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

// 把监听到的event I/O变化保存如 vector
// events_是epoll_event的vector形式，通过begin() 获取首元素迭代器 然后再解引用再取地址得到epoll_event的指针，static_cast<int> c++安全换转
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    //因为多个线程会访问errno 所以用saveErrno暂时保存
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happen \n,", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    } else {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => poller updateChannel removeChannel
/**
 *      EventLoop => poller.poll
 *  ChannelList Poller
 *              ChannelMap <fd, channel*> epollfd
 * 
 * update Channel逻辑是如果index是kNew，则添加到channelMap中，并且上树，其他情况该函数只负责上树和下树，不对
 * channelMap做修改
*/
void EpollPoller::updateChannel(Channel *channel)
{
    const int index = channel -> index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n",__FUNCTION__, channel -> fd(), channel -> events(), index);

//逻辑是如果碰到新channel和之前已经被弃用的channel，则添加channel进入channelmap，因为调用方并不知道channel是否已经
//被添加进入channelmap，调用方只是更改该兴趣的事件，所以需要根据index来判断是否已经存在于channelMap channels_中 
    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel -> fd();
            channels_[fd] = channel;
        }

        channel -> set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在poller上注册过了
    {
        int fd = channel -> fd();
        if (channel -> isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel -> set_index(kDeleted);
        }
        else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

/**
 * 从poller中删除channel 如果channel已经是kNew或者kDelted，先从ChannelMap中删除，如果是kAdded那么必须再多一步
 * 下树，最后set_index = kNew是是为了说明 该event已经下树，随时准备再上树
 * 该操作只是删除poll中的channelMap eventLoop中不会涉及
*/
void EpollPoller::removeChannel(Channel* channel)
{
    int fd = channel -> fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel -> index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel -> set_index(kNew);
}

//填写活跃的链接 
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel -> set_revents(events_[i].events);
        activeChannels -> push_back(channel); //EventLoop就拿到了他的poller给他返回的所有发生事件的channel列表了
    }
}
//更新channel通道 epoll_ctl add/mod/del
void EpollPoller::update(int operation, Channel* channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel -> fd();

    event.events = channel -> events();
    event.data.ptr = channel;
    event.data.fd = fd; //该代码错误，data是union集合 只能赋值一次，所以不能同时赋予ptr和fd 这里只是和视频代码保持一致 

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error %d \n", errno);
        }
        else {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}
