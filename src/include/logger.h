#pragma once
//框架提供的日志
#include <string>

#include "asynlogqueue.h"

enum Loglevel
{
INFO,
ERROR,
};

class Logger
{
    public:
    //设置日志级别
    void SetLogLevel(Loglevel level);
    //写日志
    void Log(std::string msg);
    //获取日志单例
    static Logger& GetInstance();
    private:
    Logger();
    Logger(const Logger&) =delete;
    Logger(Logger&&) = delete;

    int m_loglevel; //记录日志级别
    AsynLogQueue<std::string> m_alQue;
};

//定义宏LOG_INFO("xxxxxx %d %s",200,"xxx")

#define LOG_INFO(logmsgformat, ...)             \
do                                              \
{                                               \
    Logger& logger = Logger::GetInstance();     \
    logger.SetLogLevel(INFO);                   \
    char c[1024] = {0};                         \
    snprintf(c,1024,logmsgformat,##__VA_ARGS__);\
    logger.Log(c);                              \
}while(0);                                    

#define LOG_ERR(logmsgformat, ...)              \
do                                              \
{                                               \
    Logger& logger = Logger::GetInstance();     \
    logger.SetLogLevel(ERROR);                  \
    char c[1024] = {0};                         \
    snprintf(c,1024,logmsgformat,##__VA_ARGS__);\
    logger.Log(c);                              \
}while(0);                                   