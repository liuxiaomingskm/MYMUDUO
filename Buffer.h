#pragma once


#include <vector>
#include <string>
#include <algorithm>


//网络底层的缓冲器类型定义
/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
///非阻塞编程中，buffer还是非常必要的，无论是读写，因为tcp的缓冲区是有限的，又因为是非阻塞的，（即陆续发送完）所以需要自己维护一个buffer，这样才能保证数据的完整性
class Buffer 

{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

// 即使没有inline，编译器也会自动将其作为inline构造函数
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
        {}

    size_t readableBytes() const 
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    //返回可读数据的起始位置
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    // onMessage string <- Buffer
    void retrieve(size_t len) // retrieve 更像是一个移动指针的操作
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; //应用只读取了刻度缓冲区的一部分数据，就是len，还剩下readerIndex_ += len -> writerIndex_的数据
        }
        else { // len == readableBytes()
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;

    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); //应用可读取数据的长度
    }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); //上面一句把缓冲区可读的数据已经读取出来，这里肯定要对缓冲区进行复位操作 也就是更新
        return result;
    }

    // buffer_.size() - writerIndex_  len
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容函数
        }
    }

    // 把[data, data+len)的数据写入到writable缓冲区当中
    // 无论是从fd上读取数据 或者把数据写入buffer中，都需要写如writable对应的缓冲区
    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // 从fd上读取数据
    ssize_t readFd(int fd, int* savedErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int* savedErrno);

private:

    /**
     * buffer_.begin(): This line calls the begin() function of std::vector<char>. This function returns an iterator pointing to the first element in the vector.

&*buffer_.begin(): The asterisk (*) operator is used to dereference the iterator, yielding a reference to the first element in the vector. Then, the ampersand (&) 
operator is used to obtain the address of the first element. This effectively gives you a pointer to the beginning of the underlying array of the vector.
    */
    char* begin()
    {
        return &*buffer_.begin(); // vector底层数组首元素的地址，也就是buffer_的首地址，因为vector的begin()返回的是一个迭代器，而不是指针，所以需要加上&*，才能得到指针
    }

// 常量版本，当对象是const时，调用此版本，当是非const时，调用上面的版本
    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        // 如果可写的空间不够，就扩容 可写的字节 + （readerIndex_ - kCheaprependable）(实际就是之前已经读取的数据长度，之前读取后，readerIndex_向后移动) < len
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);      
        }
        else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_; //因为是vector，当当前对象被销毁时，会自动调用vector的析构函数，释放内存
    size_t readerIndex_;
    size_t writerIndex_;

};