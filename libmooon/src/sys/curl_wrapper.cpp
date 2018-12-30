// Writed by yijian, eyjian@qq.com or eyjian@gmail.com
#include "sys/curl_wrapper.h"
#include "utils/string_utils.h"

#if MOOON_HAVE_CURL==1
#include <curl/curl.h>
SYS_NAMESPACE_BEGIN

CHttpPostData::CHttpPostData()
    : _last(NULL), _post(NULL)
{
}

CHttpPostData::~CHttpPostData()
{
    reset();
}

void CHttpPostData::reset()
{
    if (_post != NULL)
    {
        struct curl_httppost* post = (struct curl_httppost*)_post;
        curl_formfree(post);
        _post = NULL;
    }
}

void CHttpPostData::add_content(const std::string& name, const std::string& contents, const std::string& content_type) throw (utils::CException)
{
    CURLFORMcode errcode = CURL_FORMADD_OK;

    if (content_type.empty())
    {
        // TODO warning: dereferencing type-punned pointer will break strict-aliasing rules
        errcode = curl_formadd((struct curl_httppost**)&_post, (struct curl_httppost**)&_last,
            CURLFORM_COPYNAME, name.c_str(),
            CURLFORM_COPYCONTENTS, contents.c_str(), CURLFORM_END);
    }
    else
    {
        // TODO warning: dereferencing type-punned pointer will break strict-aliasing rules
        errcode = curl_formadd((struct curl_httppost**)&_post, (struct curl_httppost**)&_last,
            CURLFORM_COPYNAME, name.c_str(),
            CURLFORM_COPYCONTENTS, contents.c_str(),
            CURLFORM_CONTENTTYPE, content_type.c_str(), CURLFORM_END);
    }

    if (errcode != CURL_FORMADD_OK)
        THROW_EXCEPTION(utils::CStringUtils::format_string("add content[%s] error", name.c_str()), errcode);
}

void CHttpPostData::add_file(const std::string& name, const std::string& filepath, const std::string& content_type) throw (utils::CException)
{
    CURLFORMcode errcode = CURL_FORMADD_OK;

    if (content_type.empty())
    {
        // TODO warning: dereferencing type-punned pointer will break strict-aliasing rules
        errcode = curl_formadd((struct curl_httppost**)&_post, (struct curl_httppost**)&_last,
            CURLFORM_COPYNAME, name.c_str(),
            CURLFORM_FILE, filepath.c_str(), CURLFORM_END);
    }
    else
    {
        // TODO warning: dereferencing type-punned pointer will break strict-aliasing rules
        errcode = curl_formadd((struct curl_httppost**)&_post, (struct curl_httppost**)&_last,
            CURLFORM_COPYNAME, name.c_str(),
            CURLFORM_FILE, filepath.c_str(),
            CURLFORM_CONTENTTYPE, content_type.c_str(), CURLFORM_END);
    }

    if (errcode != CURL_FORMADD_OK)
        THROW_EXCEPTION(utils::CStringUtils::format_string("add file[%s] error", name.c_str()), errcode);
}

////////////////////////////////////////////////////////////////////////////////
void CCurlWrapper::global_init(long flags)
{
    long flags_ = (-1 == flags)? CURL_GLOBAL_ALL: flags;

    // This function sets up the program environment that libcurl needs.
    // This function must be called at least once within a program
    // before the program calls any other function in libcurl.
    //
    // CURL_GLOBAL_ALL Initialize everything possible.
    // CURL_GLOBAL_SSL Initialize SSL.
    // CURL_GLOBAL_WIN32 Initialize the Win32 socket libraries.
    // CURL_GLOBAL_NOTHING Initialise nothing extra. This sets no bit.
    // CURL_GLOBAL_DEFAULT A sensible default. It will init both SSL and Win32. Right now, this equals the functionality of the CURL_GLOBAL_ALL mask.
    // CURL_GLOBAL_ACK_EINTR When this flag is set, curl will acknowledge EINTR condition when connecting or when waiting for data.
    curl_global_init(flags_);
}

void CCurlWrapper::global_cleanup()
{
    // This function is not thread safe.
    // This function releases resources acquired by curl_global_init.
    // You should call curl_global_cleanup once for each call you make to curl_global_init,
    // after you are done using libcurl.
    curl_global_cleanup();
}

static size_t on_write_response_header(void* buffer, size_t size, size_t nmemb, void* stream)
{
    std::string* result = reinterpret_cast<std::string*>(stream);
    result->append(reinterpret_cast<char*>(buffer), size*nmemb);
    return size * nmemb;
}

static size_t on_write_response_body(void* buffer, size_t size, size_t nmemb, void* stream)
{
    std::string* result = reinterpret_cast<std::string*>(stream);
    result->append(reinterpret_cast<char*>(buffer), size*nmemb);
    return size * nmemb;
}

static size_t on_write_response_body_into_FILE(void* buffer, size_t size, size_t nmemb, void* file)
{
    FILE* fp = static_cast<FILE*>(file);
    if (fwrite(buffer, size, nmemb, fp) != nmemb)
        THROW_SYSCALL_EXCEPTION(mooon::utils::CStringUtils::format_string("fwrite response_body error: %s", strerror(errno)), errno, "fopen");
    return size * nmemb;
}

CCurlWrapper::CCurlWrapper(
        int data_timeout_seconds, int connect_timeout_seconds, bool nosignal,
        bool keepalive, int keepidle, int keepseconds)
    throw (utils::CException)
    : _curl_version_info(NULL), _curl(NULL),
      _data_timeout_seconds(data_timeout_seconds), _connect_timeout_seconds(connect_timeout_seconds), _nosignal(nosignal),
      _keepalive(keepalive), _keepidle(keepidle), _keepseconds(keepseconds)
{
    _curl_version_info = curl_version_info(CURLVERSION_NOW);

    // Start a libcurl easy session
    // This function must be the first function to call,
    // and it returns a CURL easy handle that you must use as input to other functions in the easy interface.
    // This call MUST have a corresponding call to curl_easy_cleanup when the operation is complete.
    //
    // If you did not already call curl_global_init, curl_easy_init does it automatically.
    // This may be lethal in multi-threaded cases, since curl_global_init is not thread-safe,
    // and it may result in resource problems because there is no corresponding cleanup.
    //
    // You are strongly advised to not allow this automatic behaviour, by calling curl_global_init yourself properly.
    CURL* curl = curl_easy_init();
    if (NULL == curl)
        THROW_EXCEPTION("curl_easy_init failed", -1);
    _head_list = NULL;
    _curl = (void*)curl; // 须放在reset()之前，因为reset()有使用_curl
}

CCurlWrapper::~CCurlWrapper() throw ()
{
    // _curl
    if (_curl != NULL)
    {
        CURL* curl = (CURL*)_curl;
        curl_easy_cleanup(curl);
        _curl = NULL;
    }

    // _head_list
    if (_head_list != NULL)
    {
        curl_slist* head_list = static_cast<curl_slist*>(_head_list);
        curl_slist_free_all(head_list);
        _head_list = NULL;
    }
}

void CCurlWrapper::reset(bool clear_head_list, bool clear_cookie) throw (utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // Re-initializes a CURL handle to the default values.
    // This puts back the handle to the same state as it was in when it was just created with curl_easy_init.
    // It does not change the following information kept in the handle:
    // live connections, the Session ID cache, the DNS cache, the cookies and shares.
    curl_easy_reset(curl);

    if (clear_cookie)
    {
        errcode = curl_easy_setopt(curl, CURLOPT_COOKIE, NULL);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
    }
    if (clear_head_list && _head_list!=NULL)
    {
        curl_slist* head_list = static_cast<curl_slist*>(_head_list);
        errcode = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // Removes all traces of a previously built curl_slist linked list
        // Passing in a NULL pointer in list will make this function return immediately with no action.
        curl_slist_free_all(head_list);
        _head_list = NULL;
    }
}

bool CCurlWrapper::add_request_header(const std::string& name_value_pair)
{
    curl_slist* head_list = static_cast<curl_slist*>(_head_list);

    _head_list = curl_slist_append(head_list, name_value_pair.c_str());
    if (NULL == _head_list)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void CCurlWrapper::http_get(std::string& response_header, std::string& response_body, const std::string& url, bool enable_insecure, const char* cookie) throw (utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // 复位初始化
    reset(url, cookie, enable_insecure, on_write_response_body);

    // CURLOPT_HEADERFUNCTION
    errcode = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_WRITEDATA
    errcode = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_HTTPGET
    // 之前如何调用了非GET如POST，这个是必须的
    errcode = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    errcode = curl_easy_perform(curl);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
}

void CCurlWrapper::proxy_http_get(std::string& response_header, std::string& response_body, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure, const char* cookie) throw (utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // CURLOPT_PROXY
    errcode = curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host.c_str());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_PROXYPORT
    errcode = curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_port);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    http_get(response_header, response_body, url, enable_insecure, cookie);
}

void CCurlWrapper::http_post(const std::string& data, std::string& response_header, std::string& response_body, const std::string& url, bool enable_insecure, const char* cookie) throw (utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // 复位初始化
    reset(url, cookie, enable_insecure, on_write_response_body);

    // CURLOPT_HEADERFUNCTION
    errcode = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_WRITEDATA
    errcode = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_POSTFIELDS，需与CURLOPT_HTTPPOST区分
    errcode = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_POSTFIELDSIZE
    // if we don't provide POSTFIELDSIZE, libcurl will strlen() by itself
    errcode = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(data.size()));
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    errcode = curl_easy_perform(curl);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
}

void CCurlWrapper::http_post(const CHttpPostData* http_post_data, std::string& response_header, std::string& response_body, const std::string& url, bool enable_insecure, const char* cookie) throw (utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // 复位初始化
    reset(url, cookie, enable_insecure, on_write_response_body);

    // CURLOPT_HEADERFUNCTION
    errcode = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_WRITEDATA
    errcode = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_HTTPPOST需与CURLOPT_POSTFIELDS区分
    errcode = curl_easy_setopt(curl, CURLOPT_HTTPPOST, http_post_data->get_post());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    errcode = curl_easy_perform(curl);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
}

void CCurlWrapper::proxy_http_post(const std::string& data, std::string& response_header, std::string& response_body, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure, const char* cookie) throw (utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // CURLOPT_PROXY
    errcode = curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host.c_str());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_PROXYPORT
    errcode = curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_port);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    http_post(data, response_header, response_body, url, enable_insecure, cookie);
}

void CCurlWrapper::proxy_http_post(const CHttpPostData* http_post_data, std::string& response_header, std::string& response_body, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure, const char* cookie) throw (utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // CURLOPT_PROXY
    errcode = curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host.c_str());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_PROXYPORT
    errcode = curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_port);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    http_post(http_post_data, response_header, response_body, url, enable_insecure, cookie);
}

void CCurlWrapper::http_get_download(std::string& response_header, const std::string& local_filepath, const std::string& url, bool enable_insecure, const char* cookie) throw (sys::CSyscallException, utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;
    FILE* fp = fopen(local_filepath.c_str(), "w+");
    if (NULL == fp)
        THROW_SYSCALL_EXCEPTION(mooon::utils::CStringUtils::format_string("fopen %s error: %s", local_filepath.c_str(), strerror(errno)), errno, "fopen");

    try
    {
        // 复位初始化
        reset(url, cookie, enable_insecure, on_write_response_body_into_FILE);

        // CURLOPT_HEADERFUNCTION
        errcode = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // CURLOPT_WRITEDATA
        errcode = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // CURLOPT_HTTPGET
        // 之前如何调用了非GET如POST，这个是必须的
        errcode = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        errcode = curl_easy_perform(curl);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // 关闭文件
        FILE* fp_ = fp;
        fp = NULL;
        // fclose失败会产生内存泄漏，
        // fclose只是将数据从用户空间刷到内核空间，
        // 如果要确保数据刷到磁盘，应调用fsync(fd)，
        // close(fd)也存在同样的问题，即使返回成功，如果没有用fsync(fd)。
        if (EOF == fclose(fp_))
            THROW_SYSCALL_EXCEPTION(mooon::utils::CStringUtils::format_string("fclose %s error: %s", local_filepath.c_str(), strerror(errno)), errno, "fopen");
    }
    catch (...)
    {
        if (fp != NULL)
            fclose(fp);
    }
}

void CCurlWrapper::proxy_http_get_download(std::string& response_header, const std::string& local_filepath, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure, const char* cookie) throw (sys::CSyscallException, utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // CURLOPT_PROXY
    errcode = curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host.c_str());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_PROXYPORT
    errcode = curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_port);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    http_get_download(response_header, local_filepath, url, enable_insecure, cookie);
}

void CCurlWrapper::http_post_download(const std::string& data, std::string& response_header, const std::string& local_filepath, const std::string& url, bool enable_insecure, const char* cookie) throw (sys::CSyscallException, utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;
    FILE* fp = fopen(local_filepath.c_str(), "w+");
    if (NULL == fp)
        THROW_SYSCALL_EXCEPTION(mooon::utils::CStringUtils::format_string("fopen %s error: %s", local_filepath.c_str(), strerror(errno)), errno, "fopen");

    try
    {
        // 复位初始化
        reset(url, cookie, enable_insecure, on_write_response_body_into_FILE);

        // CURLOPT_HEADERFUNCTION
        errcode = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // CURLOPT_WRITEDATA
        errcode = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // CURLOPT_POSTFIELDS
        errcode = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // CURLOPT_POSTFIELDSIZE
        // if we don't provide POSTFIELDSIZE, libcurl will strlen() by itself
        errcode = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(data.size()));
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        errcode = curl_easy_perform(curl);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // 关闭文件
        FILE* fp_ = fp;
        fp = NULL;
        // fclose失败会产生内存泄漏，
        // fclose只是将数据从用户空间刷到内核空间，
        // 如果要确保数据刷到磁盘，应调用fsync(fd)，
        // close(fd)也存在同样的问题，即使返回成功，如果没有用fsync(fd)。
        if (EOF == fclose(fp_))
            THROW_SYSCALL_EXCEPTION(mooon::utils::CStringUtils::format_string("fclose %s error: %s", local_filepath.c_str(), strerror(errno)), errno, "fopen");
    }
    catch (...)
    {
        if (fp != NULL)
            fclose(fp);
    }
}

void CCurlWrapper::proxy_http_post_download(const std::string& data, std::string& response_header, const std::string& local_filepath, const std::string& proxy_host, uint16_t proxy_port, const std::string& url, bool enable_insecure, const char* cookie) throw (sys::CSyscallException, utils::CException)
{
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // CURLOPT_PROXY
    errcode = curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host.c_str());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_PROXYPORT
    errcode = curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_port);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    http_post_download(data, response_header, local_filepath, url, enable_insecure, cookie);
}

std::string CCurlWrapper::escape(const std::string& source)
{
    std::string result;
    CURL* curl = (CURL*)_curl;

    char *output = curl_easy_escape(curl, source.c_str(), static_cast<int>(source.length()));
    if (output != NULL)
    {
        result = output;
        curl_free(output);
    }

    return result;
}

std::string CCurlWrapper::unescape(const std::string& source_encoded)
{
    std::string result;
    CURL* curl = (CURL*)_curl;
    int outlength;

    char *output = curl_easy_unescape(curl, source_encoded.c_str(), static_cast<int>(source_encoded.length()), &outlength);
    if (output != NULL)
    {
        result = output;
        curl_free(output);
    }

    return result;
}

int CCurlWrapper::get_response_code() const throw (utils::CException)
{
    long response_code = 0;
    CURLcode errcode = curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    return static_cast<int>(response_code);
}

std::string CCurlWrapper::get_response_content_type() const throw (utils::CException)
{
    char* content_type = NULL;
    CURLcode errcode = curl_easy_getinfo(_curl, CURLINFO_CONTENT_TYPE, &content_type);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
    // If you get NULL, it means that the server didn't send a valid Content-Type header
    // or that the protocol used doesn't support this.
    // The content_type pointer will be NULL or pointing to private memory you MUST NOT free it,
    //  it gets freed when you call curl_easy_cleanup on the corresponding CURL handle.
    return (content_type!=NULL)? std::string(content_type): std::string("");
}

void CCurlWrapper::reset(const std::string& url, const char* cookie, bool enable_insecure, size_t (*on_write_response_body_into_FILE_proc)(void*, size_t, size_t, void*)) throw (utils::CException)
{
    const curl_version_info_data* curl_version_info = (curl_version_info_data*)_curl_version_info;
    CURLcode errcode;
    CURL* curl = (CURL*)_curl;

    // CURLOPT_HTTPHEADER
    // Set custom HTTP headers
    // Pass a NULL to this option to reset back to no custom headers.
    if (_head_list != NULL)
    {
        curl_slist* head_list = static_cast<curl_slist*>(_head_list);
        errcode = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head_list);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
    }

    // CURLOPT_COOKIE
    // Set contents of HTTP Cookie header
    if (cookie != NULL)
    {
        errcode = curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
    }

    // CURLOPT_NOSIGNAL
    // If onoff is 1, libcurl will not use any functions that install signal handlers or any functions that cause signals to be sent to the process.
    if (_nosignal)
    {
        // DNS解析阶段timeout的实现机制是通过SIGALRM+sigsetjmp/siglongjmp来实现的
        // 解析前通过alarm设定超时时间，并设置跳转的标记
        // 在等到超时后，进入alarmfunc函数实现跳转
        // 如果设置了CURLOPT_NOSIGNAL，则DNS解析不设置超时时间
        //
        // 为解决DNS解析不超时问题，编译时指定c-ares（一个异步DNS解析库），configure参数：
        // --enable-ares[=PATH]  Enable c-ares for DNS lookups
        const long onoff = _nosignal? 1: 0;
        errcode = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, onoff);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
    }

    // CURLOPT_TIMEOUT
    // In unix-like systems, this might cause signals to be used unless CURLOPT_NOSIGNAL is set.
    errcode = curl_easy_setopt(curl, CURLOPT_TIMEOUT, _data_timeout_seconds);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // enable TCP keep-alive for this transfer
    if (_keepalive && (curl_version_info->version_num >= 0x071900))
    {
        // Added in 7.25.0
        errcode = curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // keep-alive idle time to _keepidle seconds
        errcode = curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, _keepidle);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

        // interval time between keep-alive probes: _keepseconds seconds
        errcode = curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, _keepseconds);
        if (errcode != CURLE_OK)
            THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
    }

    // CURLOPT_SSL_VERIFYPEER
    // 相当于curl命令的“-k”或“--insecure”参数
    const int ssl_verifypeer = enable_insecure? 0: 1;
    errcode = curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, ssl_verifypeer);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_SSL_VERIFYHOST
    // When the verify value is 1,
    // curl_easy_setopt will return an error and the option value will not be changed.
    // It was previously (in 7.28.0 and earlier) a debug option of some sorts,
    // but it is no longer supported due to frequently leading to programmer mistakes.
    // Future versions will stop returning an error for 1 and just treat 1 and 2 the same.
    //
    // When the verify value is 0, the connection succeeds regardless of the names in the certificate.
    // Use that ability with caution!
    //
    // 7.28.1开始默认值为2
    int ssl_verifyhost = 0;
    if (enable_insecure)
    {
        ssl_verifyhost = 0;
    }
    else
    {
        if (curl_version_info->version_num > 0x071C00) // 7.28.0
        {
            ssl_verifyhost = 2;
        }
        else
        {
            ssl_verifyhost = 1;
        }
    }
    errcode = curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYHOST, ssl_verifyhost);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_CONNECTTIMEOUT
    // In unix-like systems, this might cause signals to be used unless CURLOPT_NOSIGNAL is set.
    errcode = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, _connect_timeout_seconds);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_READFUNCTION
    errcode = curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_HEADERFUNCTION
    errcode = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, on_write_response_header);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_WRITEFUNCTION
    if (NULL == on_write_response_body_into_FILE_proc)
        errcode = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_write_response_body);
    else
        errcode = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_write_response_body_into_FILE_proc);
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);

    // CURLOPT_URL
    errcode = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (errcode != CURLE_OK)
        THROW_EXCEPTION(curl_easy_strerror(errcode), errcode);
}

#if 0
extern "C" int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: curl_get url\n");
        exit(1);
    }
    
    try
    {
        const int timeout_seconds = 2;
        CCurlWrapper curl_wrapper(timeout_seconds);
        std::string response_body;
        curl_wrapper.get(NULL, &response_body, argv[1]);
        printf("result =>\n%s\n", response_body.c_str());

        response_body.clear();
        curl_wrapper.get(&response_body, argv[1]);
        printf("result =>\n%s\n", response_body.c_str());
    }
    catch (utils::CException& ex)
    {
        fprintf(stderr, "%s\n", ex.str().c_str());
    }
    
    return 0;
}
#endif

SYS_NAMESPACE_END
#endif // MOOON_HAVE_CURL
