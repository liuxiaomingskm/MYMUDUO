#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop* loop)
        : ownerLoop_(loop){}

bool Poller::hasChannel(Channel* channel) const{
    auto it = channels_.find(channel -> fd()); //it是ChannelMap::const_iterator迭代器
    return it != channels_.end() && it -> second == channel;
}