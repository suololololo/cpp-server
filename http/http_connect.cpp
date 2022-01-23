#include "http_connect.h"
map<std::string, std::string> users;
int http_connect::m_epollfd = -1;
int http_connect::m_user_count = 0;
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
void http_connect::init()
{
    mysql = NULL;
    byte_to_send = 0;
    byte_have_send = 0;
    m_checkstate = CHECK_STATE_REQUESTLINE;
    m_live = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_star_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    m_improv = 0;
    m_timeflag = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

void http_connect::init(int sockfd, const sockaddr_in &addr, char *root, int trigmode, int close_log,
                        std::string user, std::string password, std::string sqlname)
{
    m_sockfd = sockfd;
    m_addr = addr;
    m_trigmode = trigmode;
    addfd(m_epollfd, m_sockfd, true, m_trigmode);
    m_user_count++;
    doc_root = root;
    m_close_log = close_log;
    strcpy(m_user, user.c_str());
    strcpy(m_password, password.c_str());
    strcpy(m_sqlname, sqlname.c_str());

    init();
}
void http_connect::close_connect(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}
http_connect::LINE_STATUS http_connect::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; m_checked_idx++)
    {
        // 获取当前字节
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_connect::HTTP_CODE http_connect::parse_requestline(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    else if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }
    if (strlen(m_url) == 1)
    {
        strcat(m_url, "index.html");
    }
    m_checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_connect::HTTP_CODE http_connect::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_checkstate = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, "\t");
        if (strcasecmp(text, "keep:alive") == 0)
        {
            m_live = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, "\t");
        m_content_length = atoi(text);
    }

    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, "\t");
        m_host = text;
    }
    else
    {
        LOG_INFO("unknow header: %s", text);
    }
    return NO_REQUEST;
}

http_connect ::HTTP_CODE http_connect::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_connect::HTTP_CODE http_connect::process_read()
{
    LINE_STATUS linestatus = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;
    char *text = 0;
    while ((m_checkstate == CHECK_STATE_CONTENT && linestatus == LINE_OK) || ((linestatus = parse_line()) == LINE_OK) )
    {
        text = get_line();
        m_star_line = m_checked_idx;
        LOG_INFO("%s", text);
        switch (m_checkstate)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            retcode = parse_requestline(text);
            if (retcode == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:
        {
            retcode = parse_headers(text);
            if (retcode == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if (retcode == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            retcode = parse_content(text);
            if (retcode == GET_REQUEST)
            {
                return do_request();
            }
            linestatus = LINE_OPEN;
            break;
        }
        default:
        {
            return INTERNAL_ERROR;
        }
        }
    }
    return NO_REQUEST;
}

http_connect::HTTP_CODE http_connect::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    const char *p = strchr(m_url, '/');

    // url 请求为 /2 ...  或者 /3... 2表示登陆 3 表示注册
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcpy(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //user=123asdas&password=123 提取账户和密码
        char name[100];
        char password[100];
        int i;
        for (i = 5; m_string[i] != '&'; i++)
        {
            name[i - 5] = m_string[i];
        }
        name[i - 5] = '\0';
        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; i++, j++)
        {
            password[j] = m_string[i];
        }
        password[j] = '\0';
        //如果是注册
        if (*(p + 1) == '3')
        {
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            // 用户不存在
            if (users.find(name) == users.end())
            {
                m_lock.lock();
                
                int res = mysql_query(mysql, sql_insert);

                users.insert(std::pair<std::string, std::string>(name, password));
                m_lock.unlock();
                if (!res)
                {
                    strcpy(m_url, "/log.html");
                }
                else
                {
                    strcpy(m_url, "/registerError.html");
                }
            }
            else
            {
                strcpy(m_url, "/registerError.html");
            }
            free(sql_insert);
        }
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
            {
                strcpy(m_url, "/welcome.html");
            }
            else
                strcpy(m_url, "/logError.html");
        }
    }
    // 0 代表注册
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    // 1 代表登陆
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
    {
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len + 1);
    }
    if (stat(m_real_file, &m_file_stat) < 0)
    {
        return NO_RESOURCE;
    }
    if (!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void http_connect::process()
{
    // 读请求
    HTTP_CODE retcode = process_read();
    // 请求不完整
    if (retcode == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigmode);
        return;
    }

    bool write_ret = process_write(retcode);
    if (!write_ret)
    {
        close_connect();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigmode);
}

bool http_connect::process_write(HTTP_CODE retcode)
{
    if (retcode == INTERNAL_ERROR)
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
        {
            return false;
        }
    }
    else if (retcode == BAD_REQUEST)
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
        {
            return false;
        }
    }

    else if (retcode == FILE_REQUEST)
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            byte_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
            {
                return false;
            }
        }
    }

    else if (retcode == FORBIDDEN_REQUEST)
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_403_form))
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    byte_to_send = m_write_idx;
    return true;
}

bool http_connect::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    LOG_INFO("request %s", m_write_buf);
    return true;
}

bool http_connect::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_connect::add_content_length(int length)
{
    return add_response("content-length:%d\r\n", length);
}
bool http_connect::add_content_type()
{
    return add_response("content-type:%s\r\n", "text/html");
}
bool http_connect::add_content(const char *content)
{
    return add_response("%s", content);
}

bool http_connect::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_connect::add_linger()
{
    return add_response("Connection:%s\r\n", (m_live == true) ? "keep-alive" : "close");
}

bool http_connect::add_headers(int length)
{
    return add_content_length(length) && add_linger() && add_blank_line();
}

bool http_connect::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int byte_read = 0;
    if (m_trigmode == 0)
    {
        byte_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (byte_read <= 0)
        {
            return false;
        }
        m_read_idx += byte_read;
        return true;
    }
    else
    {
        while (true)
        {
            byte_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (byte_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                return false;
            }
            else if (byte_read == 0)
            {
                return false;
            }
            m_read_idx += byte_read;
        }
        return true;
    }
}
bool http_connect::write()
{
    if (byte_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigmode);
        init();
        return true;
    }
    int temp = 0;
    while (true)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigmode);
                return true;
            }
            unmap();
            return false;
        }

        byte_to_send -= temp;
        byte_have_send += temp;
        if (byte_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (byte_have_send - m_write_idx);
            m_iv[1].iov_len = byte_to_send;
        }
        else
        {
            m_iv[0].iov_len -= byte_have_send;
            m_iv[0].iov_base = m_write_buf + byte_have_send;
        }
        if (byte_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigmode);
            if (m_live)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

void http_connect::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

void http_connect::initmysql_result(connection_pool *pool)
{
    MYSQL *connect = NULL;
    connectionRAII mysql(&connect, pool);
    if (mysql_query(connect, "select username, password from user"))
    {
        LOG_ERROR("SELECT ERROR:%s\n", mysql_error(connect));
    }
    MYSQL_RES *result = mysql_store_result(connect);

    int num_fields = mysql_num_fields(result);
    
    MYSQL_FIELD *fields = mysql_fetch_field(result);
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);

        users[temp1] = temp2;
    }
}
