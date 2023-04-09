#pragma once
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional> // std::function

class EventLoop;
class InetAddress;

class Acceptor: noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) {

        newConnectionCallback_ = cb;
    }
  
    bool listenning() const {return listenning_;}
    void listen();

    private:
    void handleRead();

    EventLoop *loop_; // Acceptor用的就是用户定义的那个baseloop，也称作mainLoop, Tcp SERVER 直接把baseloop传给了Acceptor
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};


/**
 * A socket is an endpoint for communication between two processes or systems, usually over a network. In the context of a client-server model, s
 * ockets enable communication between a client (a user or device requesting a service) and a server (a system that provides the requested service). 
 * Here's a high-level overview of how sockets work between a client and a server:

Server-side:
a. The server creates a socket using the socket() function, specifying the communication domain (e.g., AF_INET for IPv4), socket type (
    e.g., SOCK_STREAM for TCP or SOCK_DGRAM for UDP), and protocol (usually set to 0 to use the default protocol for the specified socket type).
b. The server binds the created socket to an address (IP) and port using the bind() function.
c. For connection-oriented protocols like TCP, the server starts listening for incoming connections using the listen() function, specifying the
 maximum number of pending connections in the queue.
d. The server accepts incoming connections using the accept() function, which returns a new socket file descriptor for the established connection. 
The server can then communicate with the client through this new socket.

Client-side:
a. The client creates a socket using the socket() function, just like the server.
b. The client connects to the server using the connect() function, specifying the server's address (IP) and port.
c. Once the connection is established, the client can communicate with the server through the connected socket.

Data exchange:
a. Both the client and server can send and receive data using the send() and recv() functions (for TCP) or sendto() and recvfrom() functions 
(for UDP). They can also use higher-level I/O functions like read(), write(), or standard C library functions such as fread() and fwrite(), depending 
on the implementation.
b. The client and server can continue to exchange data until one of them decides to close the connection or an error occurs.

Connection termination:
a. For connection-oriented protocols like TCP, either the client or server can initiate the connection termination using the shutdown() function to c
lose the connection gracefully or the close() function to close the socket immediately.
b. For connectionless protocols like UDP, the client and server can simply close their sockets using the close() function when they are done exchanging data.
*/