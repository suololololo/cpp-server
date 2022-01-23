#include "utils.h"

int utils::u_epollfd = 0;
int *utils::u_pipefd = 0;
void cb_func(client_data *data)
{
    epoll_ctl(utils::u_epollfd, EPOLL_CTL_DEL, data->sockfd, 0);
    assert(data);
    close(data->sockfd);
    http_connect::m_user_count--;
}