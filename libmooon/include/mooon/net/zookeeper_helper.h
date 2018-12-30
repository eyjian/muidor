/**
 * Copyright (c) 2016, Jian Yi <eyjian at gmail dot com>
 *
 * All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MOOON_NET_ZOOKEEPER_HELPER_H
#define MOOON_NET_ZOOKEEPER_HELPER_H
#include <mooon/net/config.h>
#include <mooon/net/utils.h>
#include <mooon/sys/utils.h>
#include <mooon/utils/exception.h>
#include <mooon/utils/string_utils.h>

#if MOOON_HAVE_ZOOKEEPER == 1
#include <zookeeper/zookeeper.h>
#endif // MOOON_HAVE_ZOOKEEPER

NET_NAMESPACE_BEGIN
#if MOOON_HAVE_ZOOKEEPER == 1

// 默认zookeeper日志输出到stderr，
// 可以调用zoo_set_log_stream(FILE*)设置输出到文件中
// 还可以调用zoo_set_debug_level(ZooLogLevel)控制日志级别！！！
//
// 提供基于zookeeper的主备切换接口和读取数据等接口
//
// 使用示例：
// class CMyApplication: public mooon::net::CZookeeperHelper
// {
// public:
//     CMyApplication(const char* data);
//     void stop() { _stop = true; }
//     void run();
//     void wait();
//
// private:
//     void work();
//
// private:
//     virtual void on_zookeeper_session_connected(const char* path);
//     virtual void on_zookeeper_session_connecting(const char* path);
//     virtual void on_zookeeper_session_expired(const char *path);
//     virtual void on_zookeeper_session_event(int state, const char *path);
//     virtual void on_zookeeper_event(int type, int state, const char *path);
//
// private:
//     volatile bool _stop;
//     std::string _master_path; // 用来竞争master的zookeeper节点路径
//     std::string _master_data; // 成功竞争为master时，写入_master_path的数据，主备应当提供不同的数据，以方便判断自己是否处于主状态
// };
//
// int main(int argc, char* argv[])
// {
//     try
//     {
//         mooon::sys::g_logger = mooon::sys::create_safe_logger();
//         const std::string zk_nodes = "127.0.0.1:2181";
//         const int session_timeout_seconds = 1;
//
//         CMyApplication myapp(argv[1]);
//         myapp.create_session(zk_nodes, session_timeout_seconds);
//         myapp.run();
//         myapp.wait();
//
//         return 0;
//     }
//     catch (mooon::sys::CSyscallException& ex)
//     {
//         fprintf(stderr, "%s\n", ex.str().c_str());
//         exit(1);
//     }
//     catch (mooon::utils::CException& ex)
//     {
//         fprintf(stderr, "%s\n", ex.str().c_str());
//         exit(1);
//     }
// }
//
// CMyApplication::CMyApplication(const char* data)
//     : _stop(false)
// {
//     _master_path = "/tmp/a";
//     if (data != NULL)
//         _master_data = data;
// }
//
// void CMyApplication::run()
// {
//     // 启动时竞争master，
//     // 在成为master之前不能进入工作状态
//     while (!_stop)
//     {
//         int zk_errcode;
//         std::string zk_errmsg;
//
//         if (race_master(_master_path, _master_data, &zk_errcode, &zk_errmsg))
//         {
//             // 成为master后，
//             // 要让原来的master有足够时间退出master状态
//             MYLOG_INFO("Race master at %s with %s successfully, sleep for 10 seconds to let the old master quit\n", _master_path.c_str(), _master_data.c_str());
//             mooon::sys::CUtils::millisleep(10000);
//             MYLOG_INFO("Start working now\n");
//
//             work();
//             if (!_stop)
//             {
//                 // 退出work()，表示需要重新竞争master
//                 MYLOG_INFO("Turn to slave from master at %s with %s successfully, stop working now\n", _master_path.c_str(), _master_data.c_str());
//             }
//         }
//         else
//         {
//             // 如果node_exists_exception()返回true，表示已有master，
//             // 即_master_path已存在，返回false为其它错误，应将错误信息记录到日志
//             if (node_exists_exception(zk_errcode))
//             {
//                 MYLOG_INFO("A master exists\n");
//             }
//             else
//             {
//                 MYLOG_ERROR("Race master at %s with %s failed: (state:%d)(errcode:%d)%s\n", _master_path.c_str(), _master_data.c_str(), get_state(), zk_errcode, zk_errmsg.c_str());
//                 if (invalid_handle_exception(zk_errcode))
//                 {
//                     MYLOG_INFO("To recreate session\n");
//                     recreate_session();
//                 }
//             }
//
//             // 休息2秒后再尝试，不要过频重试，一般情况下1~10秒都是可接受的
//             mooon::sys::CUtils::millisleep(2000);
//         }
//     }
//
//     MYLOG_INFO("Exit now\n");
// }
//
// void CMyApplication::wait()
// {
// }
//
// void CMyApplication::work()
// {
//     // 要及时检查is_connected()，以防止master失效后同时存在两个master
//     while (!_stop && !is_session_expired())
//     {
//         mooon::sys::CUtils::millisleep(2000);
//         MYLOG_INFO("Working with state:\033[1;33m%d\033[m ...\n", get_state());
//     }
// }
//
// void CMyApplication::on_zookeeper_session_connected(const char* path)
// {
//     MYLOG_INFO("[\033[1;33mon_zookeeper_session_connected\033[m] path: %s\n", path);
//
//     const std::string zk_parent_path = "";
//     const std::string zk_node_name = "test";
//     const std::string zk_node_data = "123";
//
//     try
//     {
//         create_node(zk_parent_path, zk_node_name, zk_node_data, ZOO_EPHEMERAL);
//         MYLOG_INFO("Create %s/%s ok\n", zk_parent_path.c_str(), zk_node_name.c_str());
//     }
//     catch (mooon::utils::CException& ex)
//     {
//         MYLOG_ERROR("Create %s/%s failed: %s\n", zk_parent_path.c_str(), zk_node_name.c_str(), ex.str().c_str());
//     }
// }
//
// void CMyApplication::on_zookeeper_session_connecting(const char* path)
// {
//     MYLOG_INFO("[\033[1;33mon_zookeeper_session_connecting\033[m] path: %s\n", path);
// }
//
// void CMyApplication::on_zookeeper_session_expired(const char *path)
// {
//     MYLOG_INFO("[\033[1;33mon_zookeeper_session_expired\033[m] path: %s\n", path);
//     //exit(1); // 最安全的做法，在这里直接退出，通过重新启动方式再次竞争master
// }
//
// void CMyApplication::on_zookeeper_session_event(int state, const char *path)
// {
//     MYLOG_INFO("[\033[1;33mon_zookeeper_session_event\033[m][state:%d] path: %s\n", state, path);
// }
//
// void CMyApplication::on_zookeeper_event(int type, int state, const char *path)
// {
//     MYLOG_INFO("[\033[1;33mon_zookeeper_event\033[m][type:%d][state:%d] path: %s\n", type, state, path);
//
//     if (type == 3)
//     {
//         const int data_size = mooon::SIZE_4K;
//         const bool keep_watch = true;
//         std::string zk_data;
//         const int n = get_zk_data(path, &zk_data, data_size, keep_watch);
//         printf("(%d/%zd)%s\n", n, zk_data.size(), zk_data.c_str());
//     }
// }
class CZookeeperHelper
{
public:
    static bool node_exists_exception(int errcode) { return ZNODEEXISTS == errcode; }
    static bool node_not_exists_exception(int errcode) { return ZNONODE == errcode; }
    static bool invalid_handle_exception(int errcode) { return ZINVALIDSTATE == errcode; } // invalid zhandle state
    static bool connection_loss_exception(int errcode) { return ZCONNECTIONLOSS == errcode; }
    static bool not_empty_exception(int errcode) { return ZNOTEMPTY == errcode; }
    static bool session_expired_exception(int errcode) { return ZSESSIONEXPIRED == errcode; }
    static bool no_authorization_exception(int errcode) { return ZNOAUTH == errcode; }
    static bool invalid_ACL_exception(int errcode) { return ZINVALIDACL == errcode; }

public:
    CZookeeperHelper() throw ();
    virtual ~CZookeeperHelper();

    // 取zk_path的数据
    //
    // zk_data 存放存储在path的数据
    // data_size 最多获取的数据大小，注意数据的实际大小可能比data_size指定的值大
    // keep_watch 是否保持watch该path
    //
    // 如果成功返回数据实际大小，如果出错则抛异常
    int get_zk_data(const char* zk_path, std::string* zk_data, int data_size=SIZE_4K, bool keep_watch=true) const throw (utils::CException);
    std::string get_zk_data(const char* zk_path, int data_size=SIZE_4K, bool keep_watch=true) const throw (utils::CException);

    // 设置节点的数据
    void set_zk_data(const std::string& zk_path, const std::string& zk_data, struct Stat *stat=NULL, int version=-1) throw (utils::CException);

    // 取函数connect_zookeeper的参数指定的zk_nodes
    const std::string& get_zk_nodes() const throw () { return _zk_nodes; }

    // 创建与zookeeper的会话（session）
    //
    // 建立与zookeepr的连接
    // 但请注意本函数返回并不表示和zookeeper连接成功，
    // 只有成员函数on_zookeeper_connected()被回调了才表示连接成功
    //
    // zk_nodes 以逗号分隔的zookeeper节点，如：192.168.31.239:2181,192.168.31.240:2181
    // session_timeout_seconds 会话（session）超时时长（单位秒）
    // 注意实际的会话超时时长，是和服务端协商后确定的，并不仅由session_timeout_seconds决定，
    // 和服务端的配置项tickTime有关，可调用get_session_timeout()取得实际时长。
    // 服务端会话最小超时时长为minSessionTimeout（默认值为tickTime*2），
    // 服务端会话最大超时时长为maxSessionTimeout（默认值为tickTime*20），
    // 服务端的tickTime默认值为2000ms，
    // session_timeout_seconds会间于minSessionTimeout和maxSessionTimeout才有效。
    //
    // 如果报错“no port in ... Invalid argument”，是因为参数zk_nodes没有包含端口或端口号错误
    void create_session(const std::string& zk_nodes, int session_timeout_seconds=10) throw (utils::CException);

    // 关闭与zookeeper的连接
    void close_session() throw (utils::CException);

    // 重新创建与zookeeper的会话（Session），
    // 重连接之前会先关闭和释放已建立连接的资源（包括会话）
    //
    // 请注意，
    // 在调用connect_zookeeper()或reconnect_zookeeper()后，
    // 都应当重新调用race_master()去竞争成为master。
    void recreate_session() throw (utils::CException);

    // 得到当前连接的zookeeper host
    std::string get_connected_host() const throw ();
    bool get_connected_host(std::string* ip, uint16_t* port) const throw ();

    // 获取会话超时时长（单位为毫秒）
    // 注意只有is_connected()返回true时调用才有效
    int get_session_timeout() const;

    // 取得当前状态
    int get_state() const throw ();

    // 返回当前是否处于已连接状态
    bool is_connected() const throw ();

    // 返回当前是否处于正在连接状态
    bool is_connecting() const throw();

    bool is_associating() const throw();

    // 返回当前会话（session）是否超时
    bool is_session_expired() const throw();

    // 是否授权失败状态
    bool is_auth_failed() const throw();

    // 取得实际的zookeeper session超时时长，单位为秒
    int get_session_timeout_milliseconds() const;

    // 竞争成为master
    // 函数is_connected()返回true，方可调用race_master()
    //
    // zk_path 用于竞争的path，不能为空，且父path必须已经存在
    // path_data 存储到zk_path的数据，可用于识辨当前的master，因此可以考虑使用IP或其它有标识的值
    // zk_errcode 出错代码，如果zk_errcode的值为-110（）表示已有master，即node_exists_exception(zk_errcode)返回true
    // zk_errmsg 出错信息
    //
    // 由于仅基于zookeeper的ZOO_EPHEMERAL结点实现互斥，没有组合使用ZOO_SEQUENCE
    //
    // 如果竞争成功返回true，否则返回false
    bool race_master(const std::string& zk_path, const std::string& path_data, int* zk_errcode=NULL, std::string* zk_errmsg=NULL, const struct ACL_vector *acl=&ZOO_OPEN_ACL_UNSAFE) throw ();

    // 创建一个节点，要求父节点必须已经存在且有权限
    //
    // zk_parent_path 父节点的路径，值不能以“/”结尾，如果父节点为“/”则值需为空，否则报错“bad arguments”
    //
    // flags可取值：
    // 1) 0 普通节点
    // 2) ZOO_EPHEMERAL 临时节点
    // 3) ZOO_SEQUENCE 顺序节点
    // 4) ZOO_EPHEMERAL|ZOO_SEQUENCE 临时顺序节点
    //
    // acl可取值（如果值为NULL，则相当于取值ZOO_OPEN_ACL_UNSAFE）：
    // 1) ZOO_OPEN_ACL_UNSAFE 全开放，不做权限控制
    // 2) ZOO_READ_ACL_UNSAFE 只读的
    // 3) ZOO_CREATOR_ALL_ACL 创建者拥有所有权限
    void create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data, int flags, const struct ACL_vector *acl) throw (utils::CException);
    void create_node(const std::string& zk_parent_path, const std::string& zk_node_name) throw (utils::CException);
    void create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data) throw (utils::CException);
    void create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data, int flags) throw (utils::CException);
    void create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data, const struct ACL_vector *acl) throw (utils::CException);

    // 删除一个节点
    //
    // version 如果值为-1，则不做版本检查直接删除，否则因版本不致删除失败
    void delete_node(const std::string& zk_path, int version=-1) throw (utils::CException);

    // 设置ACL（Access Control List）
    //
    // acl参数可取值：
    // 1) ZOO_OPEN_ACL_UNSAFE 全开放，不做权限控制
    // 2) ZOO_READ_ACL_UNSAFE 只读的
    // 3) ZOO_CREATOR_ALL_ACL 创建者拥有所有权限
    void set_access_control_list(const std::string& zk_path, const struct ACL_vector *acl, int version=-1) throw (utils::CException);

    // 获取ACL
    void get_access_control_list(const std::string& zk_path, struct ACL_vector *acl, struct Stat *stat) throw (utils::CException);

    // 取得指定路径下的所有子节点，返回子节点个数
    int get_all_children(std::vector<std::string>* children, const std::string& zk_path, bool keep_watch=true) throw (utils::CException);

    // 将指定节点的数据存储到文件
    // data_filepath 存储数据的文件
    // zk_path 数据所在zookeeper节点完整路径
    //
    // 返回数据的字节数
    int store_data(const std::string& data_filepath, const std::string& zk_path, bool keep_watch=true) throw (sys::CSyscallException, utils::CException);

public: // 仅局限于被zk_watcher()调用，其它情况均不应当调用
    void zookeeper_session_connected(const char* path);
    void zookeeper_session_connecting(const char* path);
    void zookeeper_session_expired(const char *path);
    void zookeeper_session_event(int state, const char *path);
    void zookeeper_event(int type, int state, const char *path);

private:
    // zookeeper session连接成功事件
    virtual void on_zookeeper_session_connected(const char* path) {}

    // zookeeper session正在建立连接事件
    virtual void on_zookeeper_session_connecting(const char* path) {}

    // zookeeper session过期事件
    //
    // 过期后可以调用recreate_session()重建立与zookeeper的会话
    // 注意重连接后得重新调用race_master()竞争master，
    // 简单点的做法是session过期后退出当前进程，通过重新启动的方式来竞争master
    //
    // 特别注意（session过期的前提是连接已经建立）：
    // 如果连接被挂起，不会触发on_zookeeper_session_expired()事件，
    // 当用于选master时，调用者需要自己维护这种情况下的超时，
    // 以避免临时节点被删除后仍然保持为master状态。
    virtual void on_zookeeper_session_expired(const char *path) {}

    // session类zookeeper事件
    virtual void on_zookeeper_session_event(int state, const char *path) {}

    // 非session类zookeeper事件，包括但不限于：
    // 1) 节点被删除（state值为3，type值为2，即type值为ZOO_DELETED_EVENT）
    // 2) 节点的数据被修改（state值为3，type值为3，即type值为ZOO_CHANGED_EVENT）
    //
    // path 触发事件的path，如：/tmp/a
    virtual void on_zookeeper_event(int type, int state, const char *path) {}

private:
    const clientid_t* _zk_clientid;
    zhandle_t* _zk_handle; // zookeeper句柄
    std::string _zk_nodes; // 连接zookeeper的节点字符串
    int _expected_session_timeout_seconds; // 期待的zookeeper会话超时时长
    int _physical_session_timeout_seconds; // 实际的zookeeper会话超时时长
    time_t _start_connect_time; // 开始连接时间，如果连接成功值为0
};

inline static void zk_watcher(zhandle_t *zh, int type, int state, const char *path, void *context)
{
    CZookeeperHelper* self = static_cast<CZookeeperHelper*>(context);

    if (type != ZOO_SESSION_EVENT)
    {
        self->zookeeper_event(type, state, path);
    }
    else
    {
        // 0: 初始状态（也就是没有状态的状态）
        // 1: ZOO_CONNECTING_STATE
        // 2: ZOO_ASSOCIATING_STATE
        // 3: ZOO_CONNECTED_STATE
        // -112: ZOO_EXPIRED_SESSION_STATE
        // -113: ZOO_AUTH_FAILED_STATE
        // 999: NOTCONNECTED_STATE_DEF

        // 状态间转换：
        //    ZOO_CONNECTING_STATE
        // -> ZOO_ASSOCIATING_STATE
        // -> ZOO_CONNECTED_STATE/ZOO_EXPIRED_SESSION_STATE
        // -> ZOO_AUTH_FAILED_STATE

        if (ZOO_EXPIRED_SESSION_STATE == state)
        {
            // 需要重新调用zookeeper_init，简单点可以退出当前进程重启
            self->zookeeper_session_expired(path);
        }
        else if (ZOO_CONNECTED_STATE == state)
        {
            // zookeeper_init成功时type为ZOO_SESSION_EVENT，state为ZOO_CONNECTED_STATE
            self->zookeeper_session_connected(path);
        }
        else if (ZOO_CONNECTING_STATE == state)
        {
            self->zookeeper_session_connecting(path);
        }
        else
        {
            self->zookeeper_session_event(state, path);
        }
    }

    // zoo_set_watcher returns the previous watcher function
    //(void)zoo_set_watcher(zh, zk_watcher);
}

inline CZookeeperHelper::CZookeeperHelper() throw ()
    : _zk_clientid(NULL), _zk_handle(NULL),
      _expected_session_timeout_seconds(10),
      _physical_session_timeout_seconds(0),
      _start_connect_time(0)
{
}

inline CZookeeperHelper::~CZookeeperHelper()
{
    (void)close_session();
}

inline int CZookeeperHelper::get_zk_data(const char* zk_path, std::string* zk_data, int data_size, bool keep_watch) const throw (utils::CException)
{
    const int watch = keep_watch? 1: 0;
    const int data_size_ = (data_size < 1)? 1: data_size;
    struct Stat stat;
    int datalen = data_size_;

    zk_data->resize(datalen);
    const int errcode = zoo_get(_zk_handle, zk_path, watch, const_cast<char*>(zk_data->data()), &datalen, &stat);
    if (ZOK == errcode)
    {
        zk_data->resize(datalen);
        return stat.dataLength;
    }
    else
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("get path://%s data failed: %s)", zk_path, zerror(errcode)), errcode);
    }
}

inline std::string CZookeeperHelper::get_zk_data(const char* zk_path, int data_size, bool keep_watch) const throw (utils::CException)
{
    std::string zk_data;
    (void)get_zk_data(zk_path, &zk_data, data_size, keep_watch);
    return zk_data;
}

inline void CZookeeperHelper::set_zk_data(const std::string& zk_path, const std::string& zk_data, struct Stat *stat, int version) throw (utils::CException)
{
    int errcode;
    if (NULL == stat)
        errcode = zoo_set(_zk_handle, zk_path.c_str(), zk_data.data(), static_cast<int>(zk_data.size()), version);
    else
        errcode = zoo_set2(_zk_handle, zk_path.c_str(), zk_data.data(), static_cast<int>(zk_data.size()), version, stat);

    if (errcode != ZOK)
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("set path://%s data failed: %s", zk_path.c_str(), zerror(errcode)), errcode);
    }
}

inline void CZookeeperHelper::create_session(const std::string& zk_nodes, int session_timeout_seconds) throw (utils::CException)
{
    if (zk_nodes.empty())
    {
        THROW_EXCEPTION("parameter[zk_nodes] of connect_zookeeper is empty", EINVAL);
    }
    else
    {
        _expected_session_timeout_seconds = session_timeout_seconds*1000;
        _zk_nodes = zk_nodes;

        // 当连接不上时，会报如下错误（默认输出到stderr，可通过zoo_set_log_stream(FILE*)输出到文件）：
        // zk retcode=-4, errno=111(Connection refused): server refused to accept the client
        _zk_handle = zookeeper_init(_zk_nodes.c_str(), zk_watcher, _expected_session_timeout_seconds, _zk_clientid, this, 0);
        if (NULL == _zk_handle)
        {
            THROW_EXCEPTION(utils::CStringUtils::format_string("connect nodes://%s failed: %s", _zk_nodes.c_str(), strerror(errno)), errno);
        }
    }
}

inline void CZookeeperHelper::close_session() throw (utils::CException)
{
    if (_zk_handle != NULL)
    {
        int errcode = zookeeper_close(_zk_handle);

        if (errcode != ZOK)
        {
            // 出错可能是因为内存不足或网络超时等
            THROW_EXCEPTION(utils::CStringUtils::format_string("close nodes://%s failed: %s", _zk_nodes.c_str(), zerror(errcode)), errcode);
        }
        else
        {
            _zk_handle = NULL;
            _zk_clientid = NULL;
        }
    }
}

inline void CZookeeperHelper::recreate_session() throw (utils::CException)
{
    close_session();
    _zk_handle = zookeeper_init(_zk_nodes.c_str(), zk_watcher, _expected_session_timeout_seconds, _zk_clientid, this, 0);

    if (NULL == _zk_handle)
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("reconnect nodes://%s failed: %s", _zk_nodes.c_str(), strerror(errno)), errno);
    }
}

inline std::string CZookeeperHelper::get_connected_host() const throw ()
{
    std::string ip;
    uint16_t port = 0;
    get_connected_host(&ip, &port);
    return utils::CStringUtils::format_string("%s:%u", ip.c_str(), port);
}

inline bool CZookeeperHelper::get_connected_host(std::string* ip, uint16_t* port) const throw ()
{
    struct sockaddr_in6 addr_in6;
    socklen_t addr_len = sizeof(addr_in6);
    if (NULL == zookeeper_get_connected_host(_zk_handle, reinterpret_cast<struct sockaddr*>(&addr_in6), &addr_len))
    {
        return false;
    }

    const struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_in6);
    if (AF_INET == addr->sa_family)
    {
        const struct sockaddr_in* addr_in = reinterpret_cast<const struct sockaddr_in*>(addr);
        *ip = net::to_string(addr_in->sin_addr);
        *port = net::CUtils::net2host<uint16_t>(addr_in->sin_port);
    }
    else if (AF_INET6 == addr->sa_family)
    {
        *ip = net::CUtils::ipv6_tostring(addr_in6.sin6_addr.s6_addr32);
        *port = net::CUtils::net2host<uint16_t>(addr_in6.sin6_port);
    }
    else
    {
        return false;
    }

    return true;
}

inline int CZookeeperHelper::get_session_timeout() const
{
    return zoo_recv_timeout(_zk_handle);
}

inline int CZookeeperHelper::get_state() const throw ()
{
    return zoo_state(_zk_handle);
}

inline bool CZookeeperHelper::is_connected() const throw ()
{
    // 3: ZOO_CONNECTED_STATE
    const int state = zoo_state(_zk_handle);
    return (ZOO_CONNECTED_STATE == state);
}

inline bool CZookeeperHelper::is_connecting() const throw()
{
    // 1: ZOO_CONNECTING_STATE
    const int state = zoo_state(_zk_handle);
    return (ZOO_CONNECTING_STATE == state);
}

inline bool CZookeeperHelper::is_associating() const throw()
{
    // 2: ZOO_ASSOCIATING_STATE
    const int state = zoo_state(_zk_handle);
    return (ZOO_ASSOCIATING_STATE == state);
}

inline bool CZookeeperHelper::is_session_expired() const throw()
{
    // -112: ZOO_EXPIRED_SESSION_STATE
    const int state = zoo_state(_zk_handle);

    if ((ZOO_EXPIRED_SESSION_STATE == state))
    {
        return true;
    }
    else if (_start_connect_time > 0)
    {
        // 如果是zookeeper服务端被挂起，连接永久不会触发session的expired
        const time_t current_time = time(NULL);

        if (current_time > _start_connect_time)
        {
            // 增加1秒的安全值
            const int session_timeout_seconds = get_session_timeout_milliseconds();
            return static_cast<int>(current_time-_start_connect_time) > (session_timeout_seconds/1000 - 1);
        }
    }

    return false;
}

inline bool CZookeeperHelper::is_auth_failed() const throw()
{
    // -113: ZOO_AUTH_FAILED_STATE
    const int state = zoo_state(_zk_handle);
    return (ZOO_AUTH_FAILED_STATE == state);
}

inline int CZookeeperHelper::get_session_timeout_milliseconds() const
{
    return zoo_recv_timeout(_zk_handle);
}

inline bool CZookeeperHelper::race_master(const std::string& zk_path, const std::string& path_data, int* zk_errcode, std::string* zk_errmsg, const struct ACL_vector *acl) throw ()
{
    // ZOO_EPHEMERAL|ZOO_SEQUENCE
    // 调用之前，需要确保_zk_path的父路径已存在
    // (-4)connection loss，比如为zookeeper_init指定了无效的hosts（一个有效的host也没有）
    const int errcode_ = zoo_create(_zk_handle, zk_path.c_str(), path_data.c_str(), static_cast<int>(path_data.size()), acl, ZOO_EPHEMERAL, NULL, 0);

    if (ZOK == errcode_)
    {
        if (zk_errcode != NULL)
            *zk_errcode = errcode_;
        if (zk_errmsg != NULL)
            *zk_errmsg = "SUCCESS";
        return true;
    }
    else
    {
        if (zk_errcode != NULL)
            *zk_errcode = errcode_;
        if (zk_errmsg != NULL)
            *zk_errmsg = zerror(errcode_);
        // ZOK operation completed successfully
        // ZNONODE the parent node does not exist
        // ZNODEEXISTS the node already exists
        return false;
    }
}

inline void CZookeeperHelper::create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data, int flags, const struct ACL_vector *acl) throw (utils::CException)
{
    const struct ACL_vector *acl_ = (NULL == acl)? &ZOO_OPEN_ACL_UNSAFE: acl;
    const std::string& zk_path = zk_parent_path + std::string("/") + zk_node_name;
    const int errcode = zoo_create(_zk_handle, zk_path.c_str(), zk_node_data.c_str(), static_cast<int>(zk_node_data.size()), acl_, flags, NULL, 0);
    if (errcode != ZOK)
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("create path://%s failed: %s", zk_path.c_str(), zerror(errcode)), errcode);
    }
}

inline void CZookeeperHelper::create_node(const std::string& zk_parent_path, const std::string& zk_node_name) throw (utils::CException)
{
    const std::string zk_node_data;
    const int flags = -1;
    const struct ACL_vector *acl = NULL;
    create_node(zk_parent_path, zk_node_name, zk_node_data, flags, acl);
}

inline void CZookeeperHelper::create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data) throw (utils::CException)
{
    const int flags = -1;
    const struct ACL_vector *acl = NULL;
    create_node(zk_parent_path, zk_node_name, zk_node_data, flags, acl);
}

inline void CZookeeperHelper::create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data, int flags) throw (utils::CException)
{
    const struct ACL_vector *acl = NULL;
    create_node(zk_parent_path, zk_node_name, zk_node_data, flags, acl);
}

inline void CZookeeperHelper::create_node(const std::string& zk_parent_path, const std::string& zk_node_name, const std::string& zk_node_data, const struct ACL_vector *acl) throw (utils::CException)
{
    const int flags = -1;
    create_node(zk_parent_path, zk_node_name, zk_node_data, flags, acl);
}

inline void CZookeeperHelper::delete_node(const std::string& zk_path, int version) throw (utils::CException)
{
    const int errcode = zoo_delete(_zk_handle, zk_path.c_str(), version);
    if (errcode != ZOK)
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("delete path://%s failed: %s", zk_path.c_str(), zerror(errcode)), errcode);
    }
}

inline void CZookeeperHelper::set_access_control_list(const std::string& zk_path, const struct ACL_vector *acl, int version) throw (utils::CException)
{
    const int errcode = zoo_set_acl(_zk_handle, zk_path.c_str(), version, acl);
    if (errcode != ZOK)
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("set path://%s ACL failed: %s", zk_path.c_str(), zerror(errcode)), errcode);
    }
}

inline void CZookeeperHelper::get_access_control_list(const std::string& zk_path, struct ACL_vector *acl, struct Stat *stat) throw (utils::CException)
{
    const int errcode = zoo_get_acl(_zk_handle, zk_path.c_str(), acl, stat);
    if (errcode != ZOK)
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("get path://%s ACL failed: %s", zk_path.c_str(), zerror(errcode)), errcode);
    }
}

inline int CZookeeperHelper::get_all_children(std::vector<std::string>* children, const std::string& zk_path, bool keep_watch) throw (utils::CException)
{
    struct String_vector strings;
    const int watch = keep_watch? 1: 0;
    const int errcode = zoo_get_children(_zk_handle, zk_path.c_str(), watch, &strings);
    if (errcode != ZOK)
    {
        THROW_EXCEPTION(utils::CStringUtils::format_string("get path://%s children failed: %s", zk_path.c_str(), zerror(errcode)), errcode);
    }

    children->resize(strings.count);
    for (int i=0; i<strings.count; ++i)
    {
        (*children)[i] = strings.data[i];
        //free(strings.data[i]);
    }
    deallocate_String_vector(&strings);
    return static_cast<int>(children->size());
}

inline int CZookeeperHelper::store_data(const std::string& data_filepath, const std::string& zk_path, bool keep_watch) throw (sys::CSyscallException, utils::CException)
{
    int errcode = 0;
    std::string zk_data;
    int n = get_zk_data(zk_path.c_str(), &zk_data, mooon::SIZE_8K);
    if (n > mooon::SIZE_8K)
        n = get_zk_data(zk_path.c_str(), &zk_data, n);

    const int fd = open(data_filepath.c_str(), O_WRONLY|O_CREAT|O_TRUNC, FILE_DEFAULT_PERM);
    if (-1 == fd)
    {
        errcode = errno;
        THROW_SYSCALL_EXCEPTION(utils::CStringUtils::format_string("open file://%s with write|create|truncate failed: %s", data_filepath.c_str(), strerror(errcode)), errcode, "open");
    }
    else
    {
        const int n = write(fd, zk_data.data(), zk_data.size());

        if (n != static_cast<int>(zk_data.size()))
        {
            errcode = errno;
            close(fd);
            THROW_SYSCALL_EXCEPTION(utils::CStringUtils::format_string("write file://%s failed: %s", data_filepath.c_str(), strerror(errcode)), errcode, "write");
        }
        else
        {
            if (close(fd) != 0)
            {
                errcode = errno;
                THROW_SYSCALL_EXCEPTION(utils::CStringUtils::format_string("close file://%s failed: %s", data_filepath.c_str(), strerror(errcode)), errcode, "close");
            }

            return n;
        }
    }
}

inline void CZookeeperHelper::zookeeper_session_connected(const char* path)
{
    _zk_clientid = zoo_client_id(_zk_handle);
    _physical_session_timeout_seconds = get_session_timeout_milliseconds();
    _start_connect_time = 0;
    this->on_zookeeper_session_connected(path);
}

inline void CZookeeperHelper::zookeeper_session_connecting(const char* path)
{
    _start_connect_time = time(NULL);
    this->on_zookeeper_session_connecting(path);
}

inline void CZookeeperHelper::zookeeper_session_expired(const char* path)
{
    this->on_zookeeper_session_expired(path);
}

inline void CZookeeperHelper::zookeeper_session_event(int state, const char *path)
{
    this->on_zookeeper_session_event(state, path);
}

inline void CZookeeperHelper::zookeeper_event(int type, int state, const char *path)
{
    this->on_zookeeper_event(type, state, path);
}

#endif // MOOON_HAVE_ZOOKEEPER == 1
NET_NAMESPACE_END
#endif // MOOON_NET_ZOOKEEPER_HELPER_H
