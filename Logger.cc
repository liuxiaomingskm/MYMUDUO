#include <iostream>
#include <string>

#include "Logger.h"
#include "Timestamp.h"


//获取日志唯一实例
Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

//设置日志级别
void Logger::setLogLevel(LogLevel level){
    logLevel_ = level;
}

//写日志 [级别信息] time ： msg
void Logger::log(std::string msg){
    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    
    //打印时间和msg
    std::cout << Timestamp::now().toString() <<" : " << msg << std::endl;
}
