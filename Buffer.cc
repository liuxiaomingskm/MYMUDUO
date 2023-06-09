#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 从fd上读取数据 Poller工作在LT模式下
 * Buffer缓冲区是有大小的，但是从fd上读取数据的时候，却不知道tcp数据最终的大小
 *
*/

ssize_t Buffer::readFd(int fd, int *savedErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间 64k

    struct iovec vec[2];

    const size_t writable = writableBytes(); // 这是Buffer底层缓冲区剩余的可写空间的大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
  // when there is enough space in this buffer, don't read into extrabuf.
  // when extrabuf is used, we read 128k-1 bytes at most.
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; // 两个缓冲区都有空间，就使用两个缓冲区，否则只使用一个缓冲区 当writable 大于 extrabuf时，把数据读入extrabuf意义不大，因为extrabuf只有64k
  // when extrabuf is used, we read 128k-1 bytes at most.
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (static_cast<size_t>(n) <= writable) //Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了数据 
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // writerIndex_ 开始写n - writeable大小的数据
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *savedErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *savedErrno = errno;
    }
    return n;
}