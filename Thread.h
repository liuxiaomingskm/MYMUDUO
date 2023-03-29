#pragma once

#include "noncopyable.h"
// #include "Poller.h"

#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>
#include <functional>

// typedef struct _log_line {
//     char *data;
//     int len;
//     int fd;
// } log_line;

// struct _log_line;
// extern _log_line *log_line_head;

class Thread : noncopyable {
    public:
        using ThreadFunc = std::function<void()>; //函数指针

        explicit Thread(ThreadFunc, const std::string& name = std::string());
        ~Thread();

        void start();
        void join();

        bool started() const { return started_; }
        pid_t tid() const {return tid_;}
        const std::string& name() const { return name_;}

        static int numCreated() {return numCreated_;}
        private:
        void setDefaultName();

        bool started_;
        bool joined_; //当前线程等待其他线程
        std::shared_ptr<std::thread> thread_;
        pid_t tid_;
        ThreadFunc func_;
        std::string name_;
        static std::atomic_int numCreated_;

};
