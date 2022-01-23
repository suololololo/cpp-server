
#include "config.h"
int main(int argc, char *argv[])
{
    std::string user = "root";
    std::string password = "295747";
    std::string databaseName = "web";
    Config config;
    config.parse_arg(argc, argv);
    server ser;
    ser.init(config.PORT, user, password, databaseName, config.LOG_WRITE, config.OPT_LINGER, config.TRIGMODE,
             config.SQL_NUM, config.THREAD_NUM, config.CLOSE_LOG, config.ACTOR_MODEL);
    ser.init_log();
    ser.init_sql();
    ser.init_pool();

    ser.set_trigmode();
    ser.event_listen();
    ser.start();

    return 0;
}