#ifndef UTILS_H
#define UTILS_H
#include "../timer/timer.h"
#include "../http/http_connect.h"
#include <fcntl.h>
#include <sys/epoll.h>
#include <assert.h>
#include <signal.h>
class utils
{
public:
    utils()
    {
    }
    ~utils()
    {
    }
    void init(int timeslot)
    {
        m_time = timeslot;
    }

    int setnoblocking(int fd)
    {
        int old_option = fcntl(fd, F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_option);
        return old_option;
    }
    //将事件注册到内核事件表中
    void addfd(int epollfd, int fd, bool one_shot, int trigmode)
    {
        epoll_event event;
        event.data.fd = fd;
        if (trigmode == 1)
        {
            event.events = EPOLLET | EPOLLRDHUP | EPOLLIN;
        }
        else
        {
            event.events = EPOLLIN | EPOLLRDHUP;
        }
        if (one_shot)
        {
            // 防止多个线程同时处理一个socket 的事件
            event.events |= EPOLLONESHOT;
        }
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        setnoblocking(fd);
    }
    void removefd(int epollfd, int fd)
    {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
        close(fd);
    }
    void modfd(int epollfd, int fd, int ev, int trigmode)
    {
        epoll_event event;
        event.data.fd = fd;
        if (trigmode == 1)
        {
            event.events = ev | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
        }
        else
        {
            event.events = ev | EPOLLIN | EPOLLRDHUP | EPOLLONESHOT;
        }
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    }

    void show_error(int sockfd, const char *info)
    {
        send(sockfd, info, strlen(info), 0);
        close(sockfd);
    }
    // 定时处理任务 同时重新定时触发SIGALRM信号
    void timer_handler()
    {
        m_timer_list.trick();
        alarm(m_time);
    }
    //信号函数
    void addsig(int sig, void(handler)(int), bool restart = true)
    {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;
        if (restart)
        {
            sa.sa_flags |= SA_RESTART;
        }
        sigfillset(&sa.sa_mask);
        assert(sigaction(sig, &sa, NULL) != -1);
    }
    //信号处理函数
    static void sig_handler(int sig)
    {
        int save_errno = errno;
        int msg = sig;
        send(u_pipefd[1], (char *)&msg, 1, 0);
        errno = save_errno;
    }

public:
    static int u_epollfd;
    int m_time;
    sort_timer_list m_timer_list;
    static int *u_pipefd;
};

void cb_func(client_data *data);
#endif