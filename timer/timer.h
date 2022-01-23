//  双向链表实现定时器
#ifndef TIMER_H
#define TIMER_H
#include <time.h>
#include <arpa/inet.h>
class timer_util;
struct client_data
{
    int sockfd;
    sockaddr_in address;
    timer_util *timer;
};
class timer_util
{
public:
    timer_util() : prev(NULL), next(NULL){};


public:
    time_t expire;
    void (*cb_function)(client_data *); //回调函数
    timer_util *prev;
    timer_util *next;
    client_data *user_data;
};

class sort_timer_list
{
public:
    sort_timer_list() : head(NULL), tail(NULL) {}
    ~sort_timer_list();
    void add_timer(timer_util *timer);
    // 调整定时器位置 只考虑时间变大的情况
    void adjust_timer(timer_util *timer);
    void del_timer(timer_util *timer);
    void trick();

private:
    void add_timer(timer_util *timer, timer_util *next);

private:
    timer_util *head;
    timer_util *tail;
    
};
#endif