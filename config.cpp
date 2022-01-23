#include "config.h"
Config::Config()
{
    // 默认监听端口
    PORT = 3456;
    // 日志写入方式 默认同步
    LOG_WRITE = 0;
    //电平模式， 默认 LT + LT
    TRIGMODE = 0;
    // 优雅关闭连接 默认关闭
    OPT_LINGER = 0;
    // 数据库连接数量 默认8
    SQL_NUM = 8;
    // 线程数量 默认8
    THREAD_NUM = 8;
    //是否关闭日志 默认开启
    CLOSE_LOG = 0;
    // 并发模型 默认 proactor
    ACTOR_MODEL = 0;
}
void Config::parse_arg(int argc, char *argv[])
{
    const char *str = "p:l:m:o:s:t:c:a:";
    int opt;
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            PORT = atoi(optarg);
            break;
        }
        case 'l':
        {
            LOG_WRITE = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIGMODE = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            SQL_NUM = atoi(optarg);
            break;
        }
        case 't':
        {
            THREAD_NUM = atoi(optarg);
            break;
        }
        case 'c':
        {
            CLOSE_LOG = atoi(optarg);
            break;
        }
        case 'a':
        {
            ACTOR_MODEL = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}