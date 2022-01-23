#include "connect_pool.h"

connection_pool::connection_pool()
{
    m_curconnect = 0;
    m_freeconnect = 0;
}
connection_pool::~connection_pool()
{
    destroy_pool();
}

void connection_pool::init(string url, string databasename, string user, string password, int port,
                           int maxconncet, int close_log)
{
    m_url = url;
    m_database_name = databasename;
    m_user = user;
    m_password = password;
    m_port = port;
    m_close_log = close_log;

    for (int i = 0; i < maxconncet; i++)
    {
        MYSQL *connect = NULL;
        connect = mysql_init(connect);
        if (connect == NULL)
        {
            LOG_ERROR("MYSQL ERROR, INIT CONNECT ERROR");
            exit(1);
        }
        connect = mysql_real_connect(connect, m_url.c_str(), m_user.c_str(), m_password.c_str(),
                                     m_database_name.c_str(), port, NULL, 0);
        if (connect == NULL)
        {
            LOG_ERROR("MYSQL ERROR, CONNECT ERROR");
            exit(1);
        }
        connect_list.push_back(connect);
        m_freeconnect++;
    }
    m_maxconnect = m_freeconnect;
    reserve = sem(m_freeconnect);
}
connection_pool *connection_pool::get_instance()
{
    static connection_pool instance;
    return &instance;
}

MYSQL *connection_pool::get_connection()
{
    if (connect_list.size() == 0)
    {
        return NULL;
    }
    MYSQL *connect;
    reserve.wait();
    m_lock.lock();
    connect = connect_list.front();
    connect_list.pop_front();

    m_freeconnect--;
    m_curconnect++;
    m_lock.unlock();
    return connect;
}

bool connection_pool::release_connection(MYSQL *conn)
{
    if (conn == NULL)
    {
        return false;
    }

    m_lock.lock();
    connect_list.push_back(conn);
    m_curconnect--;
    m_freeconnect++;
    m_lock.unlock();
    reserve.post();
    return true;
}

int connection_pool::get_free_connection()
{
    return m_freeconnect;
}

void connection_pool::destroy_pool()
{
    m_lock.lock();
    if (connect_list.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = connect_list.begin(); it != connect_list.end(); it++)
        {
            MYSQL *connect = *it;
            mysql_close(connect);
        }
        m_curconnect = 0;
        m_freeconnect = 0;
        connect_list.clear();
    }
    m_lock.unlock();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *p)
{
    *SQL = p->get_connection();
    connect = *SQL;
    pool = p;
}

connectionRAII::~connectionRAII()
{
    pool->release_connection(connect);
}