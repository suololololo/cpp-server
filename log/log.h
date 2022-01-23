#ifndef LOG_H
#define LOG_H
#include "../util/blockqueue.h"
#include <string>
#include <iostream>
#include <stdio.h>

class log
{
public:

    static log *get_instance()
    {
        static log instance;
        return &instance;
    }
    static void *flush_log_thread(void *arg)
    {
        log::get_instance()->async_write_log();
    }

    //log 文件名， 日志缓冲区大小， 最大行数，最长日志队列
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
    void write_log(int level, const char *format, ...);
    void flush(void);

private:
    log();
    virtual ~log();
    void *async_write_log()
    {
        std::string log;
        while (m_log_queue->pop(log))
        {
            m_mutex.lock();
            fputs(log.c_str(), m_log);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; // 文件路径
    char log_name[128]; // log文件名
    block_queue<std::string> *m_log_queue;
    locker m_mutex;
    FILE *m_log; // 打开log文件指针
    bool m_async = true;
    int m_close_log;
    int m_log_buf_size;
    int m_splite_lines; // 最大行数
    char *m_buf;
    int m_today;       // 按天分日志
    long long m_count; //日志行数记录
};
#define LOG_DEBUG(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        log::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        log::get_instance()->flush();                             \
    }

#define LOG_INFO(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        log::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        log::get_instance()->flush();                             \
    }
#define LOG_WARN(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        log::get_instance()->write_log(2, format, ##__VA_ARGS__); \
        log::get_instance()->flush();                             \
    }
#define LOG_ERROR(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        log::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        log::get_instance()->flush();                             \
    }

#endif