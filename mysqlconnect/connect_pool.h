#ifndef CONNECT_POOL_H
#define CONNECT_POOL_H
#include <mysql/mysql.h>
#include <string>
#include <list>
#include "../log/log.h"
using namespace std;
class connection_pool
{
private:
    connection_pool();
    ~connection_pool();

public:
    MYSQL *get_connection();
    bool release_connection(MYSQL *conn);
    int get_free_connection();
    void destroy_pool();
    static connection_pool *get_instance();
    void init(string url, string databasename, string user, string password, int port, int maxconncet, int close_log);

private:
    string m_user;
    string m_password;
    int m_port;
    string m_url;
    string m_database_name;
    int m_close_log;
    int m_maxconnect;  // 最大连接
    int m_curconnect;  // 当前使用连接
    int m_freeconnect; //空闲连接
    list<MYSQL *> connect_list;
    sem reserve; //储存的连接
    locker m_lock;
};

class connectionRAII
{
public:
    connectionRAII(MYSQL **SQL, connection_pool *p);
    ~connectionRAII();

private:
    connection_pool *pool;
    MYSQL *connect;
};

#endif