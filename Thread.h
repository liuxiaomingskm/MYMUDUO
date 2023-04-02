#pragma once

#include "noncopyable.h"

#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>
#include <functional>


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
        std::shared_ptr<std::thread> thread_; // 这里不能写作Thread thread之类的 会被理解成直接创建thread 因此用shared_ptr表示
        pid_t tid_;
        ThreadFunc func_;
        std::string name_;
        static std::atomic_int numCreated_;

};
