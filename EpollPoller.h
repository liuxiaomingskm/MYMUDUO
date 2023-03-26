#pragma once
//需要知道Poller所有内容 因此包含该头文件而不是前置声明
#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h> // epoll多路分发器

class Channel;

/**
 * epoll的使用
 * epoll_create
 * epoll_ctl add/mod/del
 * epoll_wait
*/

class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    //重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    //填写活跃的链接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel* channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};