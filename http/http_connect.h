#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <string>
#include <arpa/inet.h>
#include <string.h>
#include <map>
#include <mysql/mysql.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/unistd.h>
#include <sys/uio.h>
#include "../log/log.h"
#include "../util/utils.h"
#include "../mysqlconnect/connect_pool.h"
#include "../lock/locker.h"
// class utils;
class http_connect
{
public:
    static const int READ_BUFFER_SIZE = 2048;
    static const int FILENAME_LEN = 200;
    static const int WRITE_BUFFER_SIZE = 1024;

    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // 主状态籍 状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, // 正在分析请求行
        CHECK_STATE_HEADER,          // 分析请求头
        CHECK_STATE_CONTENT          // 分析请求文本
    };
    // 从状态及 状态   行读取状态
    enum LINE_STATUS
    {
        LINE_OK = 0, // 读取到完整的行
        LINE_BAD,    // 行出错
        LINE_OPEN,   // 行数据还不完整
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

public:
    int m_state; // 读为0 写为1
    static int m_epollfd;
    static int m_user_count;
    // 数据库相关
    MYSQL *mysql;
    int m_improv;  // 标志工作线程是否进行了操作
    int m_timeflag; // 标志是否超时

public:
    http_connect(){};
    ~http_connect(){};
    void init();
    void init(int sockfd, const sockaddr_in &addr, char *root, int trigmode, int close_log,
              std::string user, std::string password, std::string sqlname);
    void close_connect(bool real_close = true);
    bool read_once();
    bool write();
    void process();
    void initmysql_result(connection_pool *pool);
    sockaddr_in *get_address()
    {
        return &m_addr;
    }

private:
    LINE_STATUS parse_line();
    HTTP_CODE parse_requestline(char *text); // 解析请求行 获取url和请求方法
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE process_read();                            // http 请求分析入口
    HTTP_CODE do_request();                              // 处理请求 设计为连接数据库
    bool process_write(HTTP_CODE retcode);               //请求响应入口
    bool add_response(const char *format, ...);          // 将回复添加到缓冲区
    bool add_status_line(int status, const char *title); // 添加响应状态
    bool add_content_length(int length);
    bool add_content_type();
    bool add_linger();
    bool add_blank_line();
    bool add_content(const char *content);
    bool add_headers(int length);
    void unmap(); //删除文件内存映射

    char *get_line()
    {
        return m_read_buf + m_star_line;
    }

private:
    int m_sockfd;
    char m_user[100];
    char m_password[100];
    char m_sqlname[100];
    sockaddr_in m_addr;
    int m_trigmode;
    int m_close_log;
    int m_star_line; // 开始读数据的行
    char m_read_buf[READ_BUFFER_SIZE];
    int m_checked_idx; // 指向当前正在分析的字节
    int m_read_idx;    //指向最后可读字节的下一字节 即客户端尾部数据的下一字节
    char *m_url;
    METHOD m_method;
    int cgi; //POST 标志位
    char *m_version;
    int m_content_length;
    bool m_live; // 连接是否Keep alive
    char *m_host;
    char *m_string; // 储存请求报文信息

    CHECK_STATE m_checkstate;

    // static std::map<std::string, std::string> users;
    struct stat m_file_stat;
    locker m_lock;

    // // 工具类
    // utils ut;
    // 输出相关
    int m_write_idx;
    char m_write_buf[WRITE_BUFFER_SIZE];
    struct iovec m_iv[2];
    char m_real_file[FILENAME_LEN];
    char *doc_root;
    char *m_file_address;
    int m_iv_count;
    int byte_to_send;
    int byte_have_send;
    // 状态响应信息
    const char *ok_200_title = "OK";
    const char *error_400_title = "Bad Request";
    const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
    const char *error_403_title = "Forbidden";
    const char *error_403_form = "You do not have permission to get file form this server.\n";
    const char *error_404_title = "Not Found";
    const char *error_404_form = "The requested file was not found on this server.\n";
    const char *error_500_title = "Internal Error";
    const char *error_500_form = "There was an unusual problem serving the request file.\n";
};

#endif