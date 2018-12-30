// Writed by yijian on 2018/11/18
#include "net/kafka_producer.h"
#include "utils/string_utils.h"
#if MOOON_HAVE_LIBRDKAFKA==1
NET_NAMESPACE_BEGIN

class DefDeliveryReportImpl: public RdKafka::DeliveryReportCb
{
public:
    virtual ~DefDeliveryReportImpl() {}

private:
    virtual void dr_cb (RdKafka::Message &message);
};

class DefEventImpl: public RdKafka::EventCb
{
public:
    virtual ~DefEventImpl() {}

private:
    virtual void event_cb (RdKafka::Event &event);
};

class DefPartitionerImpl: public RdKafka::PartitionerCb
{
public:
    virtual ~DefPartitionerImpl() {}

private:
    virtual int32_t partitioner_cb (const RdKafka::Topic *topic, const std::string *key, int32_t partition_cnt, void *msg_opaque);
};

void DefDeliveryReportImpl::dr_cb (RdKafka::Message &message)
{
    // delivery_callback: (-192)Local: Message timed out
    // 错误原因：
    // 运行librdkafka的程序所在机器没有设置Kafka broker的hosts，
    // 也就是通过hostname找不到对应的IP地址。
    const RdKafka::ErrorCode errcode = message.err();
    const std::string& errmsg = message.errstr();
    if (RdKafka::ERR_NO_ERROR == errcode)
        MYLOG_DEBUG("delivery_callback: (%d)%s\n", errcode, errmsg.c_str());
    else
        MYLOG_ERROR("delivery_callback: (%d)%s\n", errcode, errmsg.c_str());
}

void DefEventImpl::event_cb (RdKafka::Event &event)
{
    MYLOG_DEBUG("event_callback: type(%d), severity(%d), (%d)%s\n", event.type(), event.severity(), event.err(), event.str().c_str());
}

int32_t DefPartitionerImpl::partitioner_cb (const RdKafka::Topic *topic, const std::string *key, int32_t partition_cnt, void *msg_opaque)
{
    const uint32_t hash = mooon::utils::CStringUtils::hash(key->c_str(), static_cast<int>(key->size()));
    return static_cast<int>(hash % partition_cnt);
}

CKafkaProducer::CKafkaProducer(RdKafka::DeliveryReportCb* dr_cb, RdKafka::EventCb* event_cb, RdKafka::PartitionerCb* partitioner_cb)
{
    // dr_cb
    if (NULL == dr_cb)
        _dr_cb.reset(new DefDeliveryReportImpl);
    else
        _dr_cb.reset(dr_cb);

    // event_cb
    if (NULL == event_cb)
        _event_cb.reset(new DefEventImpl);
    else
        _event_cb.reset(event_cb);

    // partitioner_cb
    if (NULL == partitioner_cb)
        _partitioner_cb.reset(new DefPartitionerImpl);
    else
        _partitioner_cb.reset(partitioner_cb);
}

bool CKafkaProducer::init(const std::string& brokers_str, const std::string& topic_str, std::string* errmsg)
{
    _brokers_str = brokers_str;
    _topic_str = topic_str;

    if (_brokers_str.empty())
    {
        if (errmsg != NULL)
            *errmsg = "no any broker specified";
        return false;
    }
    if (_topic_str.empty())
    {
        if (errmsg != NULL)
            *errmsg = "no topic specified";
        return false;
    }

    do
    {
        std::string errmsg_;
        RdKafka::Conf::ConfResult ret;
        _global_conf.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
        _topic_conf.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));

        // metadata.broker.list
        ret = _global_conf->set("metadata.broker.list", _brokers_str, errmsg_);
        if (ret != RdKafka::Conf::CONF_OK)
        {
            if (errmsg != NULL)
                *errmsg = mooon::utils::CStringUtils::format_string("set metadata.broker.list://%s error: %s", _brokers_str.c_str(), errmsg_.c_str());
            break;
        }

        // dr_cb
        ret = _global_conf->set("dr_cb", _dr_cb.get(), errmsg_);
        if (ret != RdKafka::Conf::CONF_OK)
        {
            if (errmsg != NULL)
                *errmsg = mooon::utils::CStringUtils::format_string("set dr_cb error: %s", errmsg_.c_str());
            break;
        }

        // event_cb
        ret = _global_conf->set("event_cb", _event_cb.get(), errmsg_);
        if (ret != RdKafka::Conf::CONF_OK)
        {
            if (errmsg != NULL)
                *errmsg = mooon::utils::CStringUtils::format_string("set event_cb error: %s", errmsg_.c_str());
            break;
        }

        // partitioner_cb
        ret = _topic_conf->set("partitioner_cb", _partitioner_cb.get(), errmsg_);
        if (ret != RdKafka::Conf::CONF_OK)
        {
            if (errmsg != NULL)
                *errmsg = mooon::utils::CStringUtils::format_string("set partitioner_cb error: %s", errmsg_.c_str());
            break;
        }

        // producer
        _producer.reset(RdKafka::Producer::create(_global_conf.get(), errmsg_));
        if (NULL == _producer.get())
        {
            if (errmsg != NULL)
                *errmsg = mooon::utils::CStringUtils::format_string("create producer failed: %s", errmsg_.c_str());
            break;
        }

        // topic
        _topic.reset(RdKafka::Topic::create(_producer.get(), _topic_str, _topic_conf.get(), errmsg_));
        if (NULL == _topic.get())
        {
            if (errmsg != NULL)
                *errmsg = mooon::utils::CStringUtils::format_string("create topic://%s failed: %s", _topic_str.c_str(), errmsg_.c_str());
            break;
        }

        return true;
    } while(false);

    return false;
}

int CKafkaProducer::produce(const std::string& key, const std::string& log, int32_t partition, int* errcode, std::string* errmsg)
{
    const RdKafka::ErrorCode errcode_ = _producer->produce(_topic.get(), partition, RdKafka::Producer::RK_MSG_COPY,  (void*)log.data(), log.size(), &key, NULL);

    // log可能是二进制数据，这里无法解析，所以并不适合记录到日志文件中
    if (RdKafka::ERR_NO_ERROR == errcode_)
    {
        if (errcode != NULL)
            *errcode = 0;
        if (errmsg != NULL)
            *errmsg = "SUCCESS";
        return timed_poll(0);
    }
    else
    {
        if (errcode != NULL)
            *errcode = errcode_;
        if (errmsg != NULL)
            *errmsg = err2str(errcode_);
        return -1;
    }
}

int CKafkaProducer::timed_poll(int timeout_ms)
{
    return _producer->poll(timeout_ms);
}

NET_NAMESPACE_END
#endif // MOOON_HAVE_LIBRDKAFKA
