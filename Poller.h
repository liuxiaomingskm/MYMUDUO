#pragma once

#include <unordered_map>
#include <vector>

#include "Timestamp.h"
#include "EventLoop.h"
#include "noncopyable.h"
#include "Channel.h"

class Channel;
class EventLoop;
// muduo库中多路事件分发器的核心IO复用模块
class Poller: noncopyable
 {
    public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0; //=0纯虚函数 必须被覆盖
    //remove the channel, when it destructs.Must be called in the loop thread.
    virtual void removeChannel(Channel* channel) = 0;
    //判断参数channel是否在当前Poller当中
    bool hasChannel(Channel* channel) const;

    //EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);

    protected:
    //map的key：sockfd，value：socketfd所属的channel通道类型
    using ChannelMap = std::unordered_map <int, Channel*>;
    ChannelMap channels_;
    
    private:
    EventLoop* ownerLoop_; // 定义poller所属的事件循环EventLoop
 };