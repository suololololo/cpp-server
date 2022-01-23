#include "webserver.h"
#include "log/log.h"
#include <errno.h>
#include <arpa/inet.h>
server::server()
{
    users = new http_connect[MAX_FD];
    user_timer = new client_data[MAX_FD];
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    // m_pool = new threadpool<http_connect>(actor_model, thread_num);
}

server::~server()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    delete[] user_timer;
    delete m_pool;
}

void server::init(int port, string user, string passWord, string databaseName, int log_write,
                  int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = passWord;
    m_database_name = databaseName;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_trigmode = trigmode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actor_model = actor_model;
}

void server::start()
{
    bool timeout = false;
    bool stop_server = false;
    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, m_events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = m_events[i].data.fd;
            // 新连接
            if (sockfd == m_listenfd)
            {
                bool flag = deal_client_connect();
                if (flag == false)
                {
                    continue;
                }
            }
            // 对方断开连接  或出错
            else if (m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                timer_util *t = user_timer[sockfd].timer;
                delete_timer(t, sockfd);
            }
            // 管道上有信号
            else if ((m_pipefd[0] == sockfd) && (m_events[i].events & EPOLLIN))
            {
                bool flag = deal_signal(timeout, stop_server);
                if (flag == false)
                {
                    LOG_ERROR("%s", "deal signal error");
                }
            }
            // 受到客户端的数据
            else if (m_events[i].events & EPOLLIN)
            {
                deal_with_thread(sockfd);
            }
            else if (m_events[i].events & EPOLLOUT)
            {
                deal_write(sockfd);
            }
        }
        if (timeout)
        {
            util.timer_handler();
            LOG_INFO("%s", "timer trick");
            timeout = false;
        }
    }
}

bool server::deal_client_connect()
{
    struct sockaddr_in client;
    socklen_t length = sizeof(client);
    if (m_listen_trigmode == 0)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client, &length);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is%d", "accept error", errno);
            return false;
        }
        // 连接过多
        // to do 处理连接过多的情况 定时器
        if (http_connect::m_user_count >= MAX_FD)
        {
            util.show_error(connfd, "INTERNAL SERVER BUSY");
            LOG_ERROR("%s", "INTERNAL SERVER BUSY");
            return false;
        }
        set_timer(connfd, client);
    }
    else
    {
        // 这里怎么退出循环？
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client, &length);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is%d", "accept error", errno);
                break;
            }
            // 连接过多
            // to do 处理连接过多的情况 定时器
            if (http_connect::m_user_count >= MAX_FD)
            {
                util.show_error(connfd, "INTERNAL SERVER BUSY");
                LOG_ERROR("%s", "INTERNAL SERVER BUSY");
                break;
            }
            set_timer(connfd, client);
        }
        return false;
    }
    return true;
}

void server::set_trigmode()
{
    if (m_trigmode == 0)
    {
        m_connect_trigmode = 0;
        m_listen_trigmode = 0;
    }
    else if (m_trigmode == 1)
    {
        m_connect_trigmode = 0;
        m_listen_trigmode = 1;
    }
    else if (m_trigmode == 2)
    {
        m_connect_trigmode = 1;
        m_listen_trigmode = 0;
    }
    else
    {
        m_connect_trigmode = 1;
        m_listen_trigmode = 1;
    }
}

void server::set_timer(int sockfd, struct sockaddr_in address)
{
    // 初始化http 连接
    users[sockfd].init(sockfd, address, m_root, m_connect_trigmode, m_close_log, m_user, m_password, m_database_name);

    // 初始化定时器
    user_timer[sockfd].sockfd = sockfd;
    user_timer[sockfd].address = address;
    timer_util *t = new timer_util;
    t->user_data = &user_timer[sockfd];
    t->cb_function = cb_func;
    time_t now = time(NULL);
    t->expire = now + 3 * TIMESLOT;
    user_timer[sockfd].timer = t;
    util.m_timer_list.add_timer(t);
}

void server::delete_timer(timer_util *t, int sockfd)
{
    t->cb_function(&user_timer[sockfd]);
    if (t)
    {
        util.m_timer_list.del_timer(t);
    }
    LOG_INFO("close fd %d", user_timer[sockfd].sockfd);
}

bool server::deal_signal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    char signal[1024];
    ret = recv(m_pipefd[0], signal, sizeof(signal), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }

    else
    {
        for (int i = 0; i < ret; i++)
        {
            switch (signal[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
        return true;
    }
}

void server::deal_with_thread(int sockfd)
{
    timer_util *t = user_timer[sockfd].timer;
    // reactor
    if (m_actor_model == 1)
    {
        if (t)
        {
            adjust_timer(t);
        }
        m_pool->append(users + sockfd, 0);
        while (true)
        {
            if (users[sockfd].m_improv == 1)
            {
                if (users[sockfd].m_timeflag == 1)
                {
                    delete_timer(t, sockfd);
                    users[sockfd].m_timeflag = 0;
                }
                users[sockfd].m_improv = 0;
                break;
            }
        }
    }
    // proactor
    else
    {
        // 主线程亲自读数据
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            m_pool->append_p(users + sockfd);
            if (t)
            {
                adjust_timer(t);
            }
        }
        else
        {
            delete_timer(t, sockfd);
        }
    }
}

void server::adjust_timer(timer_util *t)
{
    time_t now = time(NULL);
    t->expire = now + 3 * TIMESLOT;
    util.m_timer_list.add_timer(t);
    LOG_INFO("%s", "adjust timer once");
}

void server::deal_write(int sockfd)
{
    timer_util *t = user_timer[sockfd].timer;
    if (1 == m_actor_model)
    {
        if (t)
        {
            adjust_timer(t);
        }
        m_pool->append(users + sockfd, 1);
        while (true)
        {
            if (users[sockfd].m_improv == 1)
            {
                if (users[sockfd].m_timeflag == 1)
                {
                    delete_timer(t, sockfd);
                    users[sockfd].m_timeflag = 0;
                }
                users[sockfd].m_improv = 0;
                break;
            }
        }
    }
    else
    {
        if (users[sockfd].write())
        {
            LOG_INFO("send data to client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (t)
            {
                adjust_timer(t);
            }
            else
            {
                delete_timer(t, sockfd);
            }
        }
    }
}

void server::event_listen()
{
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    if (m_opt_linger == 0)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_port = htons(m_port);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    int flag = 1;
    // 设置重用本地地址
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = 0;
    bind(m_listenfd, (struct sockaddr *)&server_address, sizeof(server_address));
    assert(ret >= 0);

    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    util.init(TIMESLOT);

    epoll_event event[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    util.addfd(m_epollfd, m_listenfd, false, m_listen_trigmode);
    http_connect::m_epollfd = m_epollfd;
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    util.setnoblocking(m_pipefd[1]);
    util.addfd(m_epollfd, m_pipefd[0], false, 0);

    util.addsig(SIGPIPE, SIG_IGN);
    util.addsig(SIGALRM, util.sig_handler, false);
    util.addsig(SIGTERM, util.sig_handler, false);

    alarm(TIMESLOT);

    utils::u_epollfd = m_epollfd;
    utils::u_pipefd = m_pipefd;
}

void server::init_log()
{
    if (m_close_log == 0)
    {
        if (m_log_write == 1)
        {
            log::get_instance()->init("./serverLog", m_close_log, 2000, 800000, 800);
        }
        else
        {
            log::get_instance()->init("./serverLog", m_close_log, 2000, 80000, 0);
        }
    }
}

void server::init_sql()
{
    m_connection_pool = connection_pool::get_instance();
    m_connection_pool->init("localhost", m_database_name, m_user, m_password, 3306, m_sql_num, m_close_log);
    users->initmysql_result(m_connection_pool);
}

void server::init_pool()
{
    m_pool = new threadpool<http_connect>(m_actor_model, m_connection_pool, m_thread_num);
}