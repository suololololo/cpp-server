#ifndef CONFIG_H
#define CONFIG_H
#include "webserver.h"
class Config
{
public:
    Config();
    ~Config(){};
    void parse_arg(int argc, char *argv[]);

public:
    int PORT;

    int LOG_WRITE;

    int OPT_LINGER;

    int TRIGMODE;

    // 数据库连接数量
    int SQL_NUM;

    int THREAD_NUM;

    // 日志写入方式
    int CLOSE_LOG;

    int ACTOR_MODEL;
};

#endif