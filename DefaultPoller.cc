#include "Poller.h"
#include "EpollPoller.h"
//#include "PollPoller.h"

#include <stdlib.h>  // getenv()
Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if(::getenv("MUDUO_USE_POLL")) {
        return nullptr; //生成poll实例
    }else {
        return new EpollPoller(loop); //生成epoll实例
    }
}