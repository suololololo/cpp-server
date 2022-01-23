#ifndef WEBSERVER_H
#define WEBSERVER_H
#include <sys/epoll.h>
#include <signal.h>
#include "threadpool/threadpool.h"
#include "http/http_connect.h"
#include "timer/timer.h"
#include "util/utils.h"
#include "mysqlconnect/connect_pool.h"
const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5; // 最小超时单位
class server
{
public:
    server();
    ~server();
    void init(int port, string user, string passWord, string databaseName, int log_write,
              int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model);
    void start();    //程序入口
    void init_log(); // 初始化日志
    void init_sql(); // 初始化sql连接
    void init_pool();
    void set_trigmode();
    void event_listen(); // 初始化 sock epoll 添加事件监听
private:
    void set_timer(int sockfd, struct sockaddr_in address); // 记录连接 设置定时器
    void delete_timer(timer_util *t, int sockfd);
    bool deal_client_connect();
    bool deal_signal(bool &timeout, bool &stop_server);
    void deal_with_thread(int sockfd);
    void deal_write(int sockfd);
    void adjust_timer(timer_util *t); //把定时器往后调位置

private:
    int m_thread_num;
    int m_log_write; // 是否异步日志 1为异步
    int m_close_log;
    const char *m_ip;
    int m_actor_model;
    http_connect *users;

    // epoll 相关
    epoll_event m_events[MAX_EVENT_NUMBER];
    int m_port;
    int m_listenfd;
    int m_trigmode; //电平模式
    int m_epollfd;
    int m_pipefd[2];
    int m_listen_trigmode;  // 监听电平模式  0 代表LT 1 代表ET
    int m_connect_trigmode; // 连接电平模式
    int m_opt_linger;
    // 定时器
    sort_timer_list timer;
    client_data *user_timer;
    // 工具类
    utils util;

    threadpool<http_connect> *m_pool;
    char *m_root;

    // 数据库相关
    std::string m_database_name;
    std::string m_user;
    std::string m_password;

    int m_sql_num;
    connection_pool *m_connection_pool;
};

#endif