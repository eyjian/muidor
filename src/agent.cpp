/**
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
 *
 * Author: eyjian@qq.com or eyjian@gmail.com
 */
#include "protocol.h"
#include "muidor/muidor.h"
#include <fcntl.h>
#include <mooon/net/epoller.h>
#include <mooon/net/udp_socket.h>
#include <mooon/net/utils.h>
#include <mooon/sys/atomic.h>
#include <mooon/sys/close_helper.h>
#include <mooon/sys/event.h>
#include <mooon/sys/datetime_utils.h>
#include <mooon/sys/lock.h>
#include <mooon/sys/main_template.h>
#include <mooon/sys/safe_logger.h>
#include <mooon/sys/thread_engine.h>
#include <mooon/sys/utils.h>
#include <mooon/utils/args_parser.h>
#include <mooon/utils/string_utils.h>
#include <mooon/utils/tokener.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

// 是否检查magic
#define _CHECK_MAGIC_ 1

// 日志控制：
// 可通过设置环境变量MOOON_LOG_LEVEL和MOOON_LOG_SCREEN来控制日志级别和是否在屏幕上输出日志
// 1) MOOON_LOG_LEVEL可以取值debug,info,error,warn,fatal
// 2) MOOON_LOG_SCREEN取值为1表示在屏幕输出日志，其它则表示不输出

STRING_ARG_DEFINE(master_nodes, "", "master nodes, e.g., 192.168.31.66:2016,192.168.31.88:2016");
STRING_ARG_DEFINE(ip, "0.0.0.0", "listen IP");
INTEGER_ARG_DEFINE(uint16_t, port, 6200, 1000, 65535, "listen port");
INTEGER_ARG_DEFINE(uint8_t, label, 0, 0, muidor::LABEL_MAX, "unique label of a machine");
INTEGER_ARG_DEFINE(uint32_t, steps, 100000, 1, 100000000, "steps to store");

// Label过期时长参数，所有节点的expire值必须保持相同，包括master节点和所有agent节点
//
// expire用来控制Label的回收重利用，取值应当越大越好，比如可以30天则取30天，可以取7天则7天等，
// 但是过期后并不会立即被回收，而是有一个冻结期，冻结期的时间长短和expire的值相关。
//
// 当一个Label在expire指定的时间内都没有续租赁过，则会进入一段冻结期，
// 冻结期内该Label不会被回收，但也不能被租赁，在冻结期之后，该Label则会被回收
// expire值必须大于interval的两倍，且必须大10
INTEGER_ARG_DEFINE(uint32_t, expire, muidor::LABEL_EXPIRED_SECONDS, 10, 4294967295U, "label expired seconds");
// 多长间隔向master发一次租赁Lable请求
INTEGER_ARG_DEFINE(uint32_t, interval, 600, 1, 7200, "rent label interval in seconds");

////////////////////////////////////////////////////////////////////////////////
namespace muidor {

// 常量
enum
{
    SEQUENCE_BLOCK_VERSION = 1
};

#pragma pack(4)
struct SeqBlock
{
    uint32_t version;
    uint32_t label;
    uint32_t sequence;
    uint64_t timestamp;
    uint64_t magic;

    SeqBlock()
        : version(SEQUENCE_BLOCK_VERSION), label(0), sequence(0), timestamp(0), magic(0)
    {
    }

    std::string str() const
    {
        return mooon::utils::CStringUtils::format_string("block://V%u/L%u/S%u/D%s/M%" PRId64,
                version, label, sequence, mooon::sys::CDatetimeUtils::to_datetime(timestamp).c_str(), magic);
    }

    void update_label(uint32_t label_)
    {
        MYLOG_DEBUG("%s => %u\n", str().c_str(), label_);
        label = label_;
    }

    void update_magic()
    {
        if (timestamp >= sequence+label+version)
            magic = timestamp - (sequence+label+version);
        else
            magic = (sequence+label+version) - timestamp;
    }

    bool valid_magic() const
    {
        if (timestamp >= sequence+label+version)
            return magic == timestamp - (sequence+label+version);
        else
            return magic == (sequence+label+version) - timestamp;
    }
};
#pragma pack()

class CUidAgent: public mooon::sys::CMainHelper
{
public:
    CUidAgent();
    ~CUidAgent();

private:
    virtual bool on_init(int argc, char* argv[]);
    virtual bool on_run();
    virtual void on_fini();

private:
    virtual bool on_check_parameter();
    virtual void on_terminated();

private:
    void sync_thread();
    std::string get_sequence_path() const;
    int get_label(bool asynchronous);
    bool parse_master_nodes();
    bool restore_sequence();
    bool store_sequence();
    void inc_num_sequence(int n);
    uint32_t inc_sequence(uint16_t deta=1);
    uint64_t get_uniq_id(const struct MessageHead* request);
    void rent_label();
    bool label_expired() const;
    bool io_error() const { return _io_error; }
    const struct sockaddr_in& get_master_addr() const;

private:
    void prepare_response_error(int errcode);
    int prepare_response_get_label();
    int prepare_response_get_uniq_id();
    int prepare_response_get_uniq_seq();
    int prepare_response_get_label_and_seq();

private:
    int on_response_error();
    int on_response_label();

private:
    mooon::sys::CThreadEngine* _sync_thread;
    mooon::sys::CEvent _event;
    mooon::sys::CLock _lock;
    uint32_t _echo;
    std::vector<struct sockaddr_in> _masters_addr;
    mooon::net::CEpoller _epoller;
    mooon::net::CUdpSocket* _udp_socket;
    uint32_t _sequence_start;
    struct SeqBlock _seq_block;
    std::string _sequence_path;
    int _sequence_fd;
    mooon::sys::CAtomic<int> _num_sequences; // 当前累计增加数，影响fsync的调用
    time_t _current_time; // 当前时间
    time_t _last_rent_time; // 最后一次向master发起rent_label的时间
    bool _io_error; // IO出错标记，将不能继续服务

private:
    // old系列变量用来解决seq用完问题，
    // 一个小时内全部用完，则不能再提供服务，因为会导致重复的ID
    uint32_t _old_seq;
    int _old_hour;
    int _old_day;
    int _old_month;
    int _old_year;

private:
    struct sockaddr_in _from_addr;
    const struct MessageHead* _message_head;
    char _request_buffer[SOCKET_BUFFER_SIZE];
    char _response_buffer[SOCKET_BUFFER_SIZE];
    size_t _response_size;
};

extern "C" int main(int argc, char* argv[])
{
    CUidAgent agent;
    return mooon::sys::main_template(&agent, argc, argv);
}

CUidAgent::CUidAgent()
    : _sync_thread(NULL),
      _echo(0), _udp_socket(NULL),
      _sequence_fd(-1), _num_sequences(0),
      _current_time(0), _last_rent_time(0), _io_error(false),
      _old_seq(0), _old_hour(-1), _old_day(-1), _old_month(-1), _old_year(-1),
      _message_head(NULL)
{
    _sequence_start = 0;
    _sequence_path = get_sequence_path();

    memset(&_from_addr, 0, sizeof(_from_addr));
    memset(&_request_buffer, 0, sizeof(_request_buffer));
    memset(&_response_buffer, 0, sizeof(_response_buffer));
    _response_size = 0;
}

CUidAgent::~CUidAgent()
{
    delete _udp_socket;
    if (_sequence_fd != -1)
        close(_sequence_fd);
    delete _sync_thread;
}

bool CUidAgent::on_init(int argc, char* argv[])
{
    std::string errmsg;
    if (!mooon::utils::parse_arguments(argc, argv, &errmsg))
    {
        fprintf(stderr, "%s\n", errmsg.c_str());
        fprintf(stderr, "%s\n", mooon::utils::g_help_string.c_str());
        return false;
    }

    // Check parameters
    if (mooon::argument::master_nodes->value().empty() && (0 == mooon::argument::label->value()))
    {
    	fprintf(stderr, "Parameter[--master] is empty and parameter[label] is 0 at the same time.\n");
    	fprintf(stderr, "%s\n", mooon::utils::g_help_string.c_str());
    	return false;
    }
    if ((mooon::argument::expire->value() < mooon::argument::interval->value() * 2) ||
        (mooon::argument::expire->value() < mooon::argument::interval->value() + 10))
    {
        fprintf(stderr, "Parameter[--expire] should greater than interval with 10 and double\n");
        fprintf(stderr, "%s\n", mooon::utils::g_help_string.c_str());
        return false;
    }

    if (!parse_master_nodes())
    {
        return false;
    }

    try
    {
        mooon::sys::g_logger = mooon::sys::create_safe_logger();
        _current_time = time(NULL);

        _epoller.create(10);
        _udp_socket = new mooon::net::CUdpSocket;
        _udp_socket->listen(mooon::argument::ip->value(), mooon::argument::port->value(), true);
        MYLOG_INFO("Listen on %s:%d\n", mooon::argument::ip->c_value(), mooon::argument::port->value());
        _epoller.set_events(_udp_socket, EPOLLIN);

        // 从文件恢复sequence
        if (!restore_sequence()) {
            return false;
        }
        else {
            _sync_thread = new mooon::sys::CThreadEngine(mooon::sys::bind(&CUidAgent::sync_thread, this));
            return true;
        }
    }
    catch (mooon::sys::CSyscallException& ex)
    {
        fprintf(stderr, "%s\n", ex.str().c_str());
        if (mooon::sys::g_logger != NULL)
        {
            MYLOG_ERROR("%s\n", ex.str().c_str());
        }
        return false;
    }
}

bool CUidAgent::on_run()
{
    while (!to_stop())
    {
        const int milliseconds = 10000;
        int n = _epoller.timed_wait(milliseconds);

        // 不需要那么精确的时间
        _current_time = time(NULL);
        if (_current_time - _last_rent_time > static_cast<time_t>(mooon::argument::interval->value()))
        {
            // 间隔的向master发一个续租请求
            rent_label();
            _last_rent_time = _current_time;
        }

        if (0 == n)
        {
            // timeout, do nothing
        }
        else
        {
            // 循环，可以减少对CEpoller::timed_wait的调用
            for (int i=0; i<10000; ++i)
            {
                try
                {
                    // Linux 2.6.33开始支持recvmsg
                    // glibc 2.12开始支持recvmmsg
//#if __GLIBC__ == 2 && __GLIBC_MINOR__ == 12
//                    int k;
//                    const unsigned int vlen = 100;
//                    char bufs[vlen][SOCKET_BUFFER_SIZE];
//                    struct mmsghdr msgs[vlen];
//                    int n = recvmmsg(_udp_socket->get_fd(), msgs, vlen, MSG_DONTWAIT, NULL);
//#else
                    int bytes_received = _udp_socket->receive_from(_request_buffer, sizeof(_request_buffer), &_from_addr);
//#endif
                    if (-1 == bytes_received)
                    {
                        // WOULDBLOCK
                        break;
                    }
                    else if (bytes_received < static_cast<int>(sizeof(struct MessageHead)))
                    {
                        MYLOG_ERROR("Invalid size (%d) from %s: %s\n", bytes_received, mooon::net::to_string(_from_addr).c_str(), strerror(errno));
                    }
                    else
                    {
                        _message_head = reinterpret_cast<struct MessageHead*>(_request_buffer);
                        MYLOG_DEBUG("%s from %s", _message_head->str().c_str(), mooon::net::to_string(_from_addr).c_str());

                        if (bytes_received != _message_head->len)
                        {
                            MYLOG_ERROR("Invalid size (%d/%d/%zd) from %s: %s\n",
                                    bytes_received, _message_head->len.to_int(), sizeof(struct MessageHead),
                                    mooon::net::to_string(_from_addr).c_str(), strerror(errno));
                        }
                        else
                        {
                            int errcode = 0;
                            std::string errmsg;

#if _CHECK_MAGIC_ == 1
                            const uint32_t magic_ = _message_head->calc_magic();
                            if (magic_ != _message_head->magic)
                            {
                                //errcode = ERROR_ILLEGAL; // 非法来源，直接丢弃
                                MYLOG_ERROR("[%s] illegal request: %s|%u\n", mooon::net::to_string(_from_addr).c_str(), _message_head->str().c_str(), magic_);
                            }
#endif // _CHECK_MAGIC_

                            // 如果errcode在这里为非0，
                            // 则表示一个非法的包，这种情形不需做出响应
                            if (0 == errcode)
                            {
                                // Request from client
                                if (REQUEST_LABEL == _message_head->type)
                                {
                                    errcode = prepare_response_get_label();
                                }
                                else if (REQUEST_UNIQ_ID == _message_head->type)
                                {
                                    errcode = prepare_response_get_uniq_id();
                                }
                                else if (REQUEST_UNIQ_SEQ == _message_head->type)
                                {
                                    errcode = prepare_response_get_uniq_seq();
                                }
                                else if (REQUEST_LABEL_AND_SEQ == _message_head->type)
                                {
                                    errcode = prepare_response_get_label_and_seq();
                                }
                                // Response from master
                                else if (RESPONSE_ERROR == _message_head->type)
                                {
                                    if (magic_ != _message_head->magic)
                                        errcode = -1;
                                    else
                                        errcode = on_response_error();
                                }
                                else if (RESPONSE_LABEL == _message_head->type)
                                {
                                    if (magic_ != _message_head->magic)
                                        errcode = -1;
                                    else
                                        errcode = on_response_label();
                                }
                                else
                                {
                                    errcode = MUE_INVALID_TYPE;
                                    MYLOG_ERROR("Invalid message type: %s\n", _message_head->str().c_str());
                                }
                                if ((errcode != 0) && (errcode != -1))
                                {
                                    prepare_response_error(errcode);
                                }

                                // -1为Master回给Agent的响应，
                                // 其它为Client向Agent的请求，这种情形Agent需要回响应给Client
                                if (errcode != -1)
                                {
                                    try
                                    {
                                        _udp_socket->send_to(_response_buffer, _response_size, _from_addr);
                                        MYLOG_DEBUG("Send to %s ok\n", mooon::net::to_string(_from_addr).c_str());
                                    }
                                    catch (mooon::sys::CSyscallException& ex)
                                    {
                                        MYLOG_ERROR("Send to %s failed: %s\n", mooon::net::to_string(_from_addr).c_str(), ex.str().c_str());
                                    }
                                }
                            }
                        }
                    }
                }
                catch (mooon::sys::CSyscallException& ex)
                {
                    MYLOG_ERROR("Receive_from failed: %s\n", ex.str().c_str());
                    break;
                }
            } // while (true)
        } // if (0 == n)
    } // while (true)

    return true;
}

void CUidAgent::on_fini()
{
    if (_sync_thread != NULL)
        _sync_thread->join();
}

bool CUidAgent::on_check_parameter()
{
    return true;
}

void CUidAgent::on_terminated()
{
    CMainHelper::on_terminated();
}

void CUidAgent::sync_thread()
{
    while (!to_stop())
    {
        // Sleep 1s
        {
            mooon::sys::LockHelper<mooon::sys::CLock> lh(_lock);
            _event.timed_wait(_lock, 1000);
        }

        if (_sequence_fd>0 && -1==fdatasync(_sequence_fd))
        {
            MYLOG_ERROR("fdatasync failed: %s\n", strerror(errno));
            exit(1); // Fatal error
        }
    }
}

std::string CUidAgent::get_sequence_path() const
{
    return mooon::sys::CUtils::get_program_path() + std::string("/.uniq.seq");
}

int CUidAgent::get_label(bool asynchronous)
{
	if (mooon::argument::master_nodes->value().empty())
	{
		return mooon::argument::label->value();
	}
	else
	{
		struct MessageHead* request = reinterpret_cast<struct MessageHead*>(_request_buffer);
		struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_response_buffer);
		const struct sockaddr_in& master_addr = get_master_addr();

        // 遇到错误ERROR_LABEL_NOT_HOLD时，需要重试一次
        for (int k=0; k<2; ++k)
        {
            try
            {
                request->major_ver = MU_MAJOR_VERSION;
                request->minor_ver = MU_MINOR_VERSION;
                request->len = sizeof(struct MessageHead);
                request->type = REQUEST_LABEL;
                request->echo = _echo++;
                request->value1 = _seq_block.label;
                request->value2 = 0;
                request->update_magic();
                _udp_socket->send_to(_request_buffer, sizeof(struct MessageHead), master_addr);

                if (asynchronous)
                {
                    return 0;
                }
                else
                {
                    int bytes = _udp_socket->timed_receive_from(_response_buffer, sizeof(struct MessageHead), &_from_addr, 2000);
                    if (bytes != sizeof(struct MessageHead))
                    {
                        MYLOG_ERROR("timed_receive_from return %d(%d)\n", bytes, static_cast<int>(sizeof(struct MessageHead)));
                        break;
                    }

                    if (RESPONSE_ERROR == response->type)
                    {
                        MYLOG_ERROR("(%d)get label[%u] error: %s\n", k, _seq_block.label, response->str().c_str());
                        if (response->value1.to_int() != MUE_LABEL_NOT_HOLD)
                            break;

                        // 需要重新租赁Label，故重置
                        _seq_block.update_label(0);
                        continue;
                    }
                    else if ((RESPONSE_LABEL == response->type) && (response->echo == _echo-1))
                    {
                        if (response->value1.to_int() > 0)
                        {
                            // 续成功
                            _seq_block.timestamp = static_cast<uint64_t>(_current_time);
                            int label = static_cast<int>(response->value1.to_int());
                            MYLOG_INFO("rent label[%d] ok\n", label);
                            return label;
                        }
                        else
                        {
                            MYLOG_ERROR("Invalid label[%d] from %s\n", (int)response->value1.to_int(), mooon::net::to_string(_from_addr).c_str());
                            break;
                        }
                    }
                    else
                    {
                        MYLOG_ERROR("Invalid response[%s] for request[%s] from %s\n",
                                response->str().c_str(), request->str().c_str(), mooon::net::to_string(_from_addr).c_str());
                        break;
                    }
                }
            }
            catch (mooon::sys::CSyscallException& ex)
            {
                MYLOG_ERROR("Rent label from %s faield: %s\n", mooon::net::to_string(_from_addr).c_str(), ex.str().c_str());
                break;
            }
        } // for

		return -1;
	}
}

bool CUidAgent::parse_master_nodes()
{
    const std::string& master_nodes = mooon::argument::master_nodes->value();
    mooon::utils::CEnhancedTokener tokener;

    tokener.parse(master_nodes, ",", ':');
    const std::map<std::string, std::string>& tokens = tokener.tokens();
    for (std::map<std::string, std::string>::const_iterator iter=tokens.begin(); iter!=tokens.end(); ++iter)
    {
        const std::string& ip_str = iter->first;
        const std::string& port_str = iter->second;

        uint32_t ip = mooon::net::string2ipv4(ip_str);
        if (0 == ip)
        {
            fprintf(stderr, "Parameter[--master_nodes] error: %s\n", master_nodes.c_str());
            return false;
        }

        int port = atoi(port_str.c_str());
        if ((port < 1000) || (port > 65535))
        {
            fprintf(stderr, "Parameter[--master_nodes] error: %s\n", master_nodes.c_str());
            return false;
        }

        struct sockaddr_in master_addr;
        master_addr.sin_family = AF_INET;
        master_addr.sin_addr.s_addr = ip;
        master_addr.sin_port = mooon::net::CUtils::host2net(static_cast<uint16_t>(port));
        memset(master_addr.sin_zero, 0, sizeof(master_addr.sin_zero));
        _masters_addr.push_back(master_addr);
    }

    return true;
}

bool CUidAgent::restore_sequence()
{
    int label = 0;

    int fd = open(_sequence_path.c_str(), O_RDWR|O_CREAT, FILE_DEFAULT_PERM);
    if (-1 == fd)
    {
        MYLOG_ERROR("Open %s failed: %s\n", _sequence_path.c_str(), strerror(errno));
        return false;
    }

    mooon::sys::CloseHelper<int> ch(fd);
    ssize_t bytes_read = pread(fd, &_seq_block, sizeof(_seq_block), 0);
    if (0 == bytes_read)
    {
        MYLOG_INFO("%s empty\n", _sequence_path.c_str());

        label = get_label(false);
        if ((label < 1) || (label > LABEL_MAX))
        {
            MYLOG_ERROR("Invalid label[%d]\n", label);
            return false;
        }

        _sequence_fd = ch.release();
        _sequence_start = mooon::argument::steps->value();
        _seq_block.sequence = _sequence_start;
        _seq_block.update_label(static_cast<uint32_t>(label));
        return store_sequence();
    }
    else if (-1 == bytes_read)
    {
        // IO error
        MYLOG_ERROR("Read %s failed: %s\n", _sequence_path.c_str(), strerror(errno));
        return false;
    }
    else if (bytes_read != sizeof(_seq_block))
    {
        // invalid block
        MYLOG_ERROR("Read %s failed: (%zd/%zd)%s\n", _sequence_path.c_str(), bytes_read, sizeof(_seq_block), strerror(errno));
        return false;
    }
    else
    {
        // check block
        if (!_seq_block.valid_magic())
        {
            MYLOG_ERROR("%s invalid: %s\n", _seq_block.str().c_str(), _sequence_path.c_str());
            return false;
        }
        else
        {
            if (mooon::argument::master_nodes->value().empty())
            {
                // 本地模式
                label = mooon::argument::label->value();
            }
            else if (label_expired())
            {
                // 如果已过期，则需要重新租赁一个
                label = get_label(false);
                if ((label < 1) || (label > LABEL_MAX))
                {
                    MYLOG_ERROR("Invalid label[%d] from master to store\n", label);
                    return false;
                }
            }

            _sequence_fd = ch.release();
            // 多加一次steps，原因是store时未调用fsync（由sync线程异步调用）
            _sequence_start = _seq_block.sequence + (2 * mooon::argument::steps->value());
            _seq_block.sequence = _sequence_start;
            _seq_block.update_label(static_cast<uint32_t>(label));

            return store_sequence();
        }
    }
}

bool CUidAgent::store_sequence()
{
    _seq_block.update_magic();

    ssize_t byes_written = pwrite(_sequence_fd, &_seq_block, sizeof(_seq_block), 0);
    if (byes_written != sizeof(_seq_block))
    {
        _io_error = true; // 遇到IO错误时，标记为不可继续服务
        MYLOG_ERROR("Store %s to %s failed: %s\n", _seq_block.str().c_str(), _sequence_path.c_str(), strerror(errno));
        return false;
    }
    else
    {
        MYLOG_DEBUG("Store %s ok\n", _seq_block.str().c_str());

#if 1
        _sequence_start = _seq_block.sequence;
        return true;
#else
        // fsync严重影响性能
        if (-1 == fsync(_sequence_fd))
        {
            _io_error = true;
            MYLOG_ERROR("fsync %s to %s failed: %s\n", _seq_block.str().c_str(), _sequence_path.c_str(), strerror(errno));
            return false;
        }
        else
        {
            _sequence_start = _seq_block.sequence;
            MYLOG_INFO("Store %s to %s ok\n", _seq_block.str().c_str(), _sequence_path.c_str());
            return true;
        }
#endif
    }
}

void CUidAgent::inc_num_sequence(int n)
{
    if (n != -1)
    {
        ++_num_sequences;
    }
    if (-1==n || _num_sequences>=static_cast<int>(mooon::argument::steps->value()))
    {
        _event.signal();
        _num_sequences = 0;
    }
}

uint32_t CUidAgent::inc_sequence(uint16_t deta)
{
    bool stored = true;
	uint32_t sequence = 0;

	if ((_seq_block.sequence < _sequence_start) ||
	    (_seq_block.sequence - _sequence_start > mooon::argument::steps->value()))
	{
	    MYLOG_DEBUG("seq_block.sequence=%u, sequence_start=%u, steps=%u\n", _seq_block.sequence, _sequence_start, mooon::argument::steps->value());
	    stored = store_sequence();
	    inc_num_sequence(-1);
	}

	if (stored)
	{
	    if (deta <= 1)
	    {
	        // 单个时
            sequence = _seq_block.sequence++;
            if (0 == sequence)
            {
                MYLOG_INFO("sequence overflow: %u->%u\n", sequence, _seq_block.sequence);
                sequence = _seq_block.sequence++; // 排除0，原因是返回0时被当作出错
            }

            inc_num_sequence(1);
	    }
	    else
	    {
	        // 批量时
	    	if (_seq_block.sequence < _seq_block.sequence+deta)
	    	{
	    	    sequence = _seq_block.sequence;
	    	    _seq_block.sequence += deta;
	    	    inc_num_sequence(deta);
	    	}
	    	else
	    	{
	    	    sequence = 1;
	    	    _seq_block.sequence = sequence + deta;
	    	    MYLOG_INFO("Sequence overflow: %u->%u(%d)\n", sequence, _seq_block.sequence, (int)deta);
	    	    inc_num_sequence(-1);
	    	}
	    }
	}

    return sequence;
}

uint64_t CUidAgent::get_uniq_id(const struct MessageHead* request)
{
    uint32_t seq = inc_sequence();

    if (0 == seq)
    {
        return 0; // store sequence block failed
    }
    else
    {
    	static struct tm old_tm;
    	static time_t old_time = 0;

    	struct tm* now = &old_tm;
        time_t current_time = static_cast<time_t>(request->value3.to_int());
        if (0 == current_time)
        {
        	current_time = _current_time;
        }
        if (current_time - old_time > 30) // current_time != old_time
        {
        	// 由于只取小时，因此理论上每小时调用一次localtime即可
        	now = localtime(&current_time); // localtime和localtime_r开销较大
        	old_tm = *now; // Rember
        	old_time = current_time; // Rember
        }

        union UniqID uniq_id;
        uniq_id.id.user = static_cast<uint8_t>(request->value1.to_int());
        uniq_id.id.label = static_cast<uint8_t>(_seq_block.label);
        uniq_id.id.year = (now->tm_year+1900) - MU_BASE_YEAR;
        uniq_id.id.month = now->tm_mon+1;
        uniq_id.id.day = now->tm_mday;
        uniq_id.id.hour = now->tm_hour;
        uniq_id.id.seq = seq;

        if ((_old_seq > seq) &&
            (_old_hour == static_cast<int>(uniq_id.id.hour)) &&
            (_old_day == static_cast<int>(uniq_id.id.day)) &&
            (_old_month == static_cast<int>(uniq_id.id.month)) &&
            (_old_year == static_cast<int>(uniq_id.id.year)))
        {
            MYLOG_ERROR("sequence overflow\n");
            return 1; // overflow
        }
        else
        {
            _old_seq = seq;
            _old_hour = uniq_id.id.hour;
            _old_day = uniq_id.id.day;
            _old_month = uniq_id.id.month;
            _old_year = uniq_id.id.year;
            return uniq_id.value;
        }
    }
}

void CUidAgent::rent_label()
{
    if (!mooon::argument::master_nodes->value().empty())
    {
        int label = get_label(true);

        if (label > 0)
        {
            _seq_block.update_label(label);
        }
    }
}

bool CUidAgent::label_expired() const
{
    if (mooon::argument::master_nodes->value().empty())
        return false;

    bool expired = _current_time - static_cast<time_t>(_seq_block.timestamp) > static_cast<time_t>(mooon::argument::expire->value());
    if (expired)
    {
        MYLOG_ERROR("Label[%u] expired(%u): %s\n",
                _seq_block.label, mooon::argument::expire->value(),
                mooon::sys::CDatetimeUtils::to_datetime(static_cast<time_t>(_seq_block.timestamp)).c_str());
    }
    return expired;
}

// 轮询方式
const struct sockaddr_in& CUidAgent::get_master_addr() const
{
    static uint32_t i = 0;
    return _masters_addr[i++ % _masters_addr.size()];
}

void CUidAgent::prepare_response_error(int errcode)
{
    struct MessageHead* request = reinterpret_cast<struct MessageHead*>(_request_buffer);
    struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_response_buffer);

    _response_size = sizeof(struct MessageHead);
    response->major_ver = MU_MAJOR_VERSION;
    response->minor_ver = MU_MINOR_VERSION;
    response->len = sizeof(struct MessageHead);
    response->type = RESPONSE_ERROR;
    response->echo = request->echo;
    response->value1 = errcode;
    response->value2 = 0;
    response->value3 = 0;
    response->update_magic();

    MYLOG_DEBUG("prepare %s ok\n", response->str().c_str());
}

int CUidAgent::prepare_response_get_label()
{
    if (label_expired())
    {
        return MUE_LABEL_EXPIRED;
    }
    else if (io_error())
    {
        return MUE_STORE_SEQ;
    }
    else
    {
        struct MessageHead* request = reinterpret_cast<struct MessageHead*>(_request_buffer);
        struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_response_buffer);

        _response_size = sizeof(struct MessageHead);
        response->major_ver = MU_MAJOR_VERSION;
        response->minor_ver = MU_MINOR_VERSION;
        response->len = sizeof(struct MessageHead);
        response->type = RESPONSE_LABEL;
        response->echo = request->echo;
        response->value1 = _seq_block.label;
        response->value2 = 0;
        response->value3 = 0;
        response->update_magic();

        MYLOG_DEBUG("prepare %s ok\n", response->str().c_str());
        return 0;
    }
}

int CUidAgent::prepare_response_get_uniq_id()
{
    if (label_expired())
    {
        return MUE_LABEL_EXPIRED;
    }
    else if (io_error())
    {
        return MUE_STORE_SEQ;
    }
    else
    {
        struct MessageHead* request = reinterpret_cast<struct MessageHead*>(_request_buffer);
        struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_response_buffer);

        uint64_t uniq_id = get_uniq_id(request);
        if (0 == uniq_id)
        {
            return MUE_STORE_SEQ;
        }
        else if (1 == uniq_id)
        {
            return MUE_OVERFLOW;
        }
        else
        {
            _response_size = sizeof(struct MessageHead);
            response->major_ver = MU_MAJOR_VERSION;
            response->minor_ver = MU_MINOR_VERSION;
            response->len = sizeof(struct MessageHead);
            response->type = RESPONSE_UNIQ_ID;
            response->echo = request->echo;
            response->value1 = 0;
            response->value2 = 0;
            response->value3 = uniq_id; // value1和value2均为uint32_t类型，存不下uniq_id
            response->update_magic();

            MYLOG_DEBUG("Prepare %s ok\n", response->str().c_str());
            return 0;
        }
    }
}

int CUidAgent::prepare_response_get_uniq_seq()
{
    if (label_expired())
    {
        return MUE_LABEL_EXPIRED;
    }
    else if (io_error())
    {
        return MUE_STORE_SEQ;
    }
    else
    {
        struct MessageHead* request = reinterpret_cast<struct MessageHead*>(_request_buffer);
        struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_response_buffer);
        uint16_t deta = static_cast<uint16_t>(request->value1.to_int());
        uint32_t seq = inc_sequence(deta);

        if (0 == seq)
        {
            return MUE_STORE_SEQ;
        }
        else
        {
            _response_size = sizeof(struct MessageHead);
            response->major_ver = MU_MAJOR_VERSION;
            response->minor_ver = MU_MINOR_VERSION;
            response->len = sizeof(struct MessageHead);
            response->type = RESPONSE_UNIQ_SEQ;
            response->echo = request->echo;
            response->value1 = seq;
            response->value2 = 0;
            response->value3 = 0;
            response->update_magic();

            MYLOG_DEBUG("prepare %s ok\n", response->str().c_str());
            return 0;
        }
    }
}

int CUidAgent::prepare_response_get_label_and_seq()
{
    if (label_expired())
    {
        return MUE_LABEL_EXPIRED;
    }
    else if (io_error())
    {
        return MUE_STORE_SEQ;
    }
    else
    {
        struct MessageHead* request = reinterpret_cast<struct MessageHead*>(_request_buffer);
        struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_response_buffer);
        uint16_t deta = static_cast<uint16_t>(request->value1.to_int());
        uint32_t seq = inc_sequence(deta);

        if (0 == seq)
        {
            return MUE_STORE_SEQ;
        }
        else
        {
            _response_size = sizeof(struct MessageHead);
            response->major_ver = MU_MAJOR_VERSION;
            response->minor_ver = MU_MINOR_VERSION;
            response->len = sizeof(struct MessageHead);
            response->type = RESPONSE_LABEL_AND_SEQ;
            response->echo = request->echo;
            response->value1 = _seq_block.label;
            response->value2 = seq;
            response->value3 = 0;
            response->update_magic();

            MYLOG_DEBUG("prepare %s ok\n", response->str().c_str());
            return 0;
        }
    }
}

int CUidAgent::on_response_error()
{
    struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_request_buffer);
    MYLOG_ERROR("%s from %s\n", response->str().c_str(), mooon::net::to_string(_from_addr).c_str());

    if (MUE_LABEL_NOT_HOLD == response->value1.to_int())
    {
        // 需要重新租赁Label，故重置
        _seq_block.update_label(0);
        get_label(true);
    }

    return -1;
}

int CUidAgent::on_response_label()
{
    struct MessageHead* response = reinterpret_cast<struct MessageHead*>(_request_buffer);
    MYLOG_INFO("%s from %s\n", response->str().c_str(), mooon::net::to_string(_from_addr).c_str());

    uint32_t old_label = _seq_block.label;
    _seq_block.update_label(static_cast<uint32_t>(response->value1.to_int()));
    _seq_block.timestamp = static_cast<uint64_t>(_current_time);

    // Lable发生变化时，立即保存
    if (old_label != _seq_block.label)
    {
        MYLOG_DEBUG("Label change from %u to %u\n", old_label, _seq_block.label);
        (void)store_sequence();
    }

    return -1;
}

} // namespace muidor {
