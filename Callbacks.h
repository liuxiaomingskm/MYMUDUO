#pragma once

#include <memory>
#include <functional>


class Buffer;
class TcpConnection;
class Timestamp;

// In asynchronous netowrking like Muduo, where multiple parts of the application might hold references 
// to the same TcpConnection, it is important to use shared_ptr to manage the lifetime of the TcpConnection object. 
// Otherwise, the TcpConnection object might be destroyed while some other part of the application still has a reference to it.
using TcpConnectionPtr = std::shared_ptr<TcpConnection>; 
using ConnectionCallback = std::function<void (const TcpConnectionPtr&)>;
using CloseCallback = std::function<void (const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void (const TcpConnectionPtr&)>;
using HighWaterMarkCallback = std::function<void (const TcpConnectionPtr&, size_t)>;

// the data has been read to (buf, len)
using MessageCallback = std::function<void (const TcpConnectionPtr&,
                                                Buffer*,
                                                Timestamp)>;