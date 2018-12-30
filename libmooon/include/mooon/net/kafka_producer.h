// Writed by yijian on 2018/11/18
#ifndef MOOON_NET_KAFKA_PRODUCER_H
#define MOOON_NET_KAFKA_PRODUCER_H
#if MOOON_HAVE_LIBRDKAFKA==1 // 宏MOOON_HAVE_LIBRDKAFKA的值须为1
#include <mooon/net/config.h>
#include <mooon/sys/log.h>
#include <mooon/utils/scoped_ptr.h>
#include <librdkafka/rdkafkacpp.h>
NET_NAMESPACE_BEGIN

class DefDeliveryReportImpl;
class DefEventImpl;
class DefPartitionerImpl;

// 非线程安全
// 一个CKafkaProducer只能处理一个topic
class CKafkaProducer
{
public:
    // CKafkaProducer负责dr_cb、event_cb和partitioner_cb的销毁。
    //
    // 如果dr_cb为空，则使用DefDeliveryReportImpl作为DeliveryReportCb，每个消息递送成功或失败（如超时）均会调用一次dr_cb，
    // 如果event_cb为空，则使用DefEventImpl作为EventCb，
    // 如果partitioner_cb为空，则使用DefPartitionerImpl作为PartitionerCb。
    CKafkaProducer(RdKafka::DeliveryReportCb* dr_cb=NULL, RdKafka::EventCb* event_cb=NULL, RdKafka::PartitionerCb* partitioner_cb=NULL);
    bool init(const std::string& brokers_str, const std::string& topic_str, std::string* errmsg=NULL);
    int produce(const std::string& key, const std::string& log, int32_t partition=RdKafka::Topic::PARTITION_UA, int* errcode=NULL, std::string* errmsg=NULL);

    // librdkafka建议定时调用一次timed_poll，以触发数据的收发
    int timed_poll(int timeout_ms=0);

private:
    std::string _brokers_str;
    std::string _topic_str;

private:
    mooon::utils::ScopedPtr<RdKafka::Conf> _global_conf;
    mooon::utils::ScopedPtr<RdKafka::Conf> _topic_conf;
    mooon::utils::ScopedPtr<RdKafka::Producer> _producer;
    mooon::utils::ScopedPtr<RdKafka::Topic> _topic;

private:
    mooon::utils::ScopedPtr<RdKafka::DeliveryReportCb> _dr_cb;
    mooon::utils::ScopedPtr<RdKafka::EventCb> _event_cb;
    mooon::utils::ScopedPtr<RdKafka::PartitionerCb> _partitioner_cb;
};

NET_NAMESPACE_END
#endif // MOOON_HAVE_LIBRDKAFKA
#endif // MOOON_NET_KAFKA_PRODUCER_H
