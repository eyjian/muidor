// Writed by yijian (eyjian@qq.com or eyjian@gmail.com)
#include <mooon/sys/syscall_exception.h>
#include <string>
#include <vector>
#ifndef MOOON_SYS_CURL_WRAPPER_H
#define MOOON_SYS_CURL_WRAPPER_H
SYS_NAMESPACE_BEGIN

// HTTP的POST数据类
class CHttpPostData
{
public:
    CHttpPostData();
    ~CHttpPostData();

    void reset();
    const void* get_post() const { return _post; }

    void add_content(const std::string& name, const std::string& contents, const std::string& content_type=std::string("")) throw (utils::CException);

    // filepath 被上传的文件
    void add_file(const std::string& name, const std::string& filepath, const std::string& content_type=std::string("")) throw (utils::CException);

private:
    void* _last;
    void* _post;
};

// libcurl包装类
// 如果需要访问https，则在编译curl时需要指定configure的参数--with-ssl的值，值为openssl的安装目录
//
// 生成libcurl的Makefile：
// configure --prefix=/usr/local/curl-7.42.1 --enable-ares=/usr/local/c-ares --with-ssl=/usr/local/openssl # --with-libssh2=/usr/local/libssh2
//
// 一些环境还需要带上libidn（注意libidn和libidn2区别）：
// configure --prefix=/usr/local/curl-7.42.1 --enable-ares=/usr/local/c-ares --with-ssl=/usr/local/openssl --with-libidn=/usr/local/libidn # --with-libssh2=/usr/local/libssh2
//
// 编译openssl-1.0.2h：
// 1) ./config --prefix=/usr/local/openssl-1.0.2h shared threads
// 2) make depend # openssl-1.0.2h之前的版本并不需要这一步
// 3) make
// 4) make install
//
// 注意：多线程中，一个线程只能有一个CCurlWrapper对象
class CCurlWrapper
{
public:
    // 两个非线程安全函数
    // 多线程程序，应当在创建线程之前调用global_init一次
    // global_init和global_cleanup必须一对一调用
    static void global_init(long flags=-1);
    static void global_cleanup();

public:
    // 对于CGI等单线程程序，nosignal为false即可，并且也不用指定c-ares，
    // 但对于多线程使用curl的程序，则nosignal应当设定为true，并且应当指定c-ares，
    // 指定nosignal为true时，DNS解析将不带超时，为此需要configure生成Makefile时指定c-ares（c-ares是一个异步DNS解析库）
    //
    // keepalive特性要求libcurl版本不低于7.25.0，否则忽略
    CCurlWrapper(int data_timeout_seconds=2, int connect_timeout_seconds=2, bool nosignal=false, bool keepalive=false, int keepidle=120, int keepseconds=60) throw (utils::CException);
    ~CCurlWrapper() throw ();

    // 一个CCurlWrapper对象多次做不同的get或post调用时，
    // 应当在每次调用前先调用reset()清理掉上一次执行的状态。
    void reset(bool clear_head_list=true, bool clear_cookie=true) throw (utils::CException);

    // 添加http请求头名字对
    bool add_request_header(const std::string& name_value_pair);

    // HTTP GET请求
    // response_header 输出参数，存放响应的HTTP头
    // response_body 输出参数，存放响应的HTTP包体
    //
    // 重复调用之前，须先调用reset()清除上一次调用的状态
    void http_get(std::string& response_header, std::string& response_body, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (utils::CException);
    void proxy_http_get(std::string& response_header, std::string& response_body, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (utils::CException);

    // HTTP POST请求
    // 重复调用之前，须先调用reset()清除上一次调用的状态
    void http_post(const std::string& data, std::string& response_header, std::string& response_body, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (utils::CException);
    void http_post(const CHttpPostData* http_post, std::string& response_header, std::string& response_body, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (utils::CException);

    void proxy_http_post(const std::string& data, std::string& response_header, std::string& response_body, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (utils::CException);
    void proxy_http_post(const CHttpPostData* http_post, std::string& response_header, std::string& response_body, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (utils::CException);

    // GET方式下载文件
    // 注意需要处理CSyscallException异常，如果没有创建和写local_filepath权限，会抛出这个异常。
    void http_get_download(std::string& response_header, const std::string& local_filepath, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (sys::CSyscallException, utils::CException);
    void proxy_http_get_download(std::string& response_header, const std::string& local_filepath, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (sys::CSyscallException, utils::CException);

    // POST方式下载文件
    void http_post_download(const std::string& data, std::string& response_header, const std::string& local_filepath, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (sys::CSyscallException, utils::CException);
    void proxy_http_post_download(const std::string& data, std::string& response_header, const std::string& local_filepath, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure=false, const char* cookie=NULL) throw (sys::CSyscallException, utils::CException);

    std::string escape(const std::string& source);
    std::string unescape(const std::string& source_encoded);

public:
    // 取得响应的状态码，如：200、403、500等
    int get_response_code() const throw (utils::CException);
    std::string get_response_content_type() const throw (utils::CException);

private:
    // 重置操作
    void reset(const std::string& url, const char* cookie, bool enable_insecure, size_t (*on_write_response_body_into_FILE_proc)(void*, size_t, size_t, void*)) throw (utils::CException);

private:
    void* _curl_version_info; // curl_version_info_data
    void* _curl;
    void* _head_list;
    int _data_timeout_seconds;
    int _connect_timeout_seconds;
    bool _nosignal;

private:
    bool _keepalive;
    int _keepidle;
    int _keepseconds;
};

/*
 * 普通POST使用示例：
 *
#include <mooon/sys/curl_wrapper.h>
#include <iostream>

extern "C" int main(int argc, char* argv[])
{
    try
    {
        mooon::sys::CCurlWrapper curl_wrapper(2);
        std::string response_header;
        std::string response_body;

        std::string url = "http://127.0.0.1/cgi-bin/hello.cgi";
        curl_wrapper.http_post("helloX=world", response_header, response_body, url);

        std::cout << response_body << std::endl;
    }
    catch (mooon::utils::CException& ex)
    {
        std::cout << ex.str() << std::endl;
    }

    return 0;
}
*/

SYS_NAMESPACE_END
#endif // MOOON_SYS_CURL_WRAPPER_H
