#include "log.h"
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
bool log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    // 设置了阻塞队列长度 则为异步
    if (max_queue_size >= 1)
    {
        m_async = true;
        m_log_queue = new block_queue<std::string>(max_queue_size);
        pthread_t tid;
        // 后台线程调用回调函数 写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_splite_lines = split_lines;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char log_full_name[256] = {0};
    // 查找/ 最后一次出现的位置
    const char *p = strrchr(file_name, '/');
    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    m_today = my_tm.tm_mday;
    m_log = fopen(log_full_name, "a");
    if (m_log == NULL)
    {
        return false;
    }
    return true;
}

log::log()
{
    m_async = false;
    m_count = 0;
}
log::~log()
{
    if (m_log != NULL)
    {
        fclose(m_log);
    }
}

void log::write_log(int level, const char *format, ...)
{
    //日志分四个级别
    char str[16];
    switch (level)
    {
    case 0:
        strcpy(str, "[debug]:");
        break;
    case 1:
        strcpy(str, "[info]:");
        break;
    case 2:
        strcpy(str, "[warn]:");
        break;
    case 3:
        strcpy(str, "[error]:");
        break;
    default:
        strcpy(str, "[info]:");
        break;
    }

    // 获取当前时间
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    m_mutex.lock();
    m_count++;

    // 如果当前时间不是今天 或者 是一个日志的最新一行
    if (my_tm.tm_mday != m_today || m_count % m_splite_lines == 0)
    {
        char new_log[256] = {0};
        // 把之前打开的日志先写了 在关闭
        fflush(m_log);
        fclose(m_log);
        char tail[16] = {0};
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_splite_lines);
        }
        m_log = fopen(new_log, "a");
    }

    m_mutex.unlock();
    va_list valist;
    va_start(valist, format);
    std::string log_str;
    m_mutex.lock();
    // 写入缓冲区
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, str);

    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valist);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    if (m_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }
    else
    {
        //同步模式主线程亲自写
        m_mutex.lock();
        fputs(log_str.c_str(), m_log);
        m_mutex.unlock();
    }
    va_end(valist);
}

void log::flush(void)
{
    // 强制刷新写入流缓冲区
    m_mutex.lock();
    fflush(m_log);
    m_mutex.unlock();
}