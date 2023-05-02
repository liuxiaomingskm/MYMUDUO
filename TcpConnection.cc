#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string>

// 采用静态编译 和其他文件的同名函数不会冲突
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                            const std::string &nameArg,
                            int sockfd,
                            const InetAddress& localAddr,
                            const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)), 
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64*1024*1024) // 64M
      {
        // 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事情发生了，channel会回调相应的操作函数
        channel_->setReadCallback(
            std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
        );
        channel_->setWriteCallback(
            std::bind(&TcpConnection::handleWrite, this)
        );
        channel_->setCloseCallback(
            std::bind(&TcpConnection::handleClose, this)
        );
        channel_->setErrorCallback(
            std::bind(&TcpConnection::handleError, this)
        );

        LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
        socket_->setKeepAlive(true);
      }

      TcpConnection::~TcpConnection()
      {
        LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), (int)state_);
      }

      void TcpConnection::send(const std::string &buf)
      {
        if (state_==kConnected)
        {

                if (loop_->isInLoopThread()){
                    sendInLoop(buf.c_str(), buf.size()); // 这里我们传的是string而不是buffer，因此不用调用retriveAll()来重置读index的位置
                }
                else {
                    loop_->runInLoop(
                        std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size())
                    );
                }
        }
      }

      /**
       * 发送数据 应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
      */
     void TcpConnection::sendInLoop(const void* data, size_t len)
     {
        ssize_t nwrote = 0;
        size_t remaining = len;
        bool faultError = false;

        // 之前调用过该connection的shutdown，不能再进行发送了
        if (state_ == kDisconnected)
        {
            LOG_ERROR("disconnected, give up writing \n");
            return;
        }

        // 表示channel_第一次开始写数据，而且缓冲区没有待发送数据
        if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
        {
            nwrote = ::write(channel_->fd(), data, len);
            if (nwrote >= 0)
            {
                remaining = len - nwrote;
                if (remaining == 0 && writeCompleteCallback_)
                {
                    // 既然在这里数据全部发送完毕，就不用在给channel设置epollout事件了 handleWrite的前提是outputBuffer_中有待发送数据，而这里全部发送完毕了，也就不会往outputBuffer_中写数据了
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
            }
            else // nwrote < 0
            {
                nwrote = 0;
                if (errno != EWOULDBLOCK)
                {
                    LOG_ERROR("TcpConnection::sendInLoop \n");
                    if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET，客户端socket重置
                    {
                        faultError = true;
                    }
                }
            }
        }


        /**
         *  说明大概先前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区中，然后给channel
         * 注册epollout事件，poller发现tcp的发送缓冲区(不是当前的outputBuffer，而是系统自带的buffer)有空间，会通知相应的socket-channel, 调用writeCallback回调方法
         * 也就是调用TcpConnection::handleWrite方法， handleWrite方法再判断outputBuffer是否全部发送，没有发送继续发送，直到把发送缓冲区的数据全部发送完毕
        */
       if (!faultError && remaining > 0)
       {
        //目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen+remaining)
            );
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting()) 
        {
            channel_->enableWriting(); // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
        }
       }
     }

     //关闭链接
     void TcpConnection::shutdown()
     {
        if (state_ == kConnected)
        {
            setState(kDisconnecting);
            loop_->runInLoop(
                std::bind(&TcpConnection::shutdownInLoop, this)
            );
        }
     }

     void TcpConnection::shutdownInLoop()
     {
        if (!channel_->isWriting()) // 说明outputBuffer_中的数据已经全部发送完毕
        {
            socket_->shutdownWrite(); // 关闭写端
        }
     }

     // 链接建立
     void TcpConnection::connectionEstablished()
     {
        setState(kConnected);
        channel_->tie(shared_from_this());
        channel_->enableReading(); // 向poller注册channel的epollin事件

        //新链接建立，执行回调
        connectionCallback_(shared_from_this());
     }

     // 链接销毁
     void TcpConnection::connectionDestroyed()
     {
        if (state_ == kConnected)
        {
            setState(kDisconnected);
            channel_->disableAll(); // 把channel的所有该兴趣的事件，从poller中del掉
            connectionCallback_(shared_from_this());
        }
        channel_->remove(); // 把channel从poller中删除掉
     }

// handleRead, handleWrite, handleClose, handleError都是private方法，只有TcpServer类才能调用，客户端定义的回调函数为messageCallback_，connectionCallback_，
// writeCompleteCallback_，highWaterMarkCallback_，这些回调函数都是用户传入的，TcpServer类在调用handleRead, handleWrite, handleClose, handleError方法时，会调用用户传入的回调函数
     void TcpConnection::handleRead(Timestamp receiveTime)
     {
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            //已建立链接的用户，当读事件发生时，调用用户传入的回调操作onMessage
            messageCallback_(shared_from_this(),&inputBuffer_, receiveTime);
        }
        else if (n == 0){
            handleClose(); //通知了读事件，但是读到的数据为0，说明对端关闭了链接
        } else {
            errno = savedErrno;
            LOG_ERROR("TcpConnection::handleRead \n");
            handleError();
        }
     }

     void TcpConnection::handleWrite()
     {
        if (channel_->isWriting())
        {
            int savedErrno = 0;
            ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
            if (n > 0)
            {
                // n个字节已经发送出去了，缓冲区index向后移动n个字节
                outputBuffer_.retrieve(n);
                if (outputBuffer_.readableBytes() == 0) // 缓冲区中字节全部发送完毕,如果！= 0,说明这一轮还没发送完毕，继续发送
                {
                    channel_->disableWriting();
                    if (writeCompleteCallback_)
                    {
                        //唤醒loop_对应的thread线程，执行回调
                        loop_->queueInLoop(
                            std::bind(writeCompleteCallback_, shared_from_this())
                        );
                    }
                    if (state_ == kDisconnecting)
                    {
                        shutdownInLoop();
                    }
                }
            }
            else {
                LOG_ERROR("TcpConnection::handleWrite \n");
            }
        }
        else 
        {
            LOG_ERROR("TcpConnection fd = %d is down, no more writing \n", channel_->fd());
        }
     }

     //poller => channel::closeCallback => TcpConnection::handleClose
     void TcpConnection::handleClose()
     {
        LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
        setState(kDisconnected);
        channel_->disableAll();

        TcpConnectionPtr connPtr(shared_from_this());
        connectionCallback_(connPtr); //执行连接关闭的回调
        closeCallback_(connPtr); //关闭链接的回调 执行的是TcpServer::removeConnection
     }


/**
 * The handleError() function is designed to handle errors that may occur on a TCP connection. 
 * It retrieves the error code associated with the socket using the getsockopt() function and logs the error.
 * 
 *  this function might be called in response to an error event detected by the event loop, such as when an error occurs 
 * while reading from or writing to the socket, or when an unexpected disconnection occurs.
*/
     void TcpConnection::handleError()
     {
        int optval;
        socklen_t optlen = sizeof optval;
        int err = 0;
        if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        {
            err = errno;
        }
        else {
            err = optval;
        }
        LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR = %d \n", name_.c_str(),err);
     }





