#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>

namespace logpipeline {

// 消息队列类型
enum class MessageQueueType {
    KAFKA,
    RABBITMQ,
    PULSAR
};

// 消息队列消息
struct MQMessage {
    std::string topic;
    std::string key;
    std::vector<uint8_t> payload;
    uint64_t task_id = 0;
    uint64_t offset = 0;
    std::map<std::string, std::string> headers;
    std::function<void(bool, const std::string&)> callback;  // success, error_msg
};

// 消息队列生产者基类
class MessageQueueProducer {
public:
    virtual ~MessageQueueProducer() = default;
    
    // 启动生产者
    virtual bool start() = 0;
    
    // 停止生产者
    virtual void stop() = 0;
    
    // 发送消息
    virtual bool send(const MQMessage& msg) = 0;
    
    // 获取发送成功计数
    virtual uint64_t get_sent_count() const = 0;
    
    // 获取发送失败计数
    virtual uint64_t get_failed_count() const = 0;
    
    // 获取队列类型
    virtual MessageQueueType get_type() const = 0;
};

// Kafka 生产者
class KafkaProducerImpl : public MessageQueueProducer {
public:
    struct Config {
        std::vector<std::string> brokers;
        std::string version;  // "0.8", "0.11", "2.x", "3.x"
        std::map<std::string, std::string> properties;
    };
    
    explicit KafkaProducerImpl(const Config& config);
    ~KafkaProducerImpl();
    
    bool start() override;
    void stop() override;
    bool send(const MQMessage& msg) override;
    uint64_t get_sent_count() const override;
    uint64_t get_failed_count() const override;
    MessageQueueType get_type() const override {
        return MessageQueueType::KAFKA;
    }
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    Config config_;
};

// RabbitMQ 生产者
class RabbitMQProducerImpl : public MessageQueueProducer {
public:
    struct Config {
        std::vector<std::string> hosts;  // "host:port"
        std::string version;  // "3.x", "4.x"
        std::string username = "guest";
        std::string password = "guest";
        std::string vhost = "/";
        std::map<std::string, std::string> properties;
    };
    
    explicit RabbitMQProducerImpl(const Config& config);
    ~RabbitMQProducerImpl();
    
    bool start() override;
    void stop() override;
    bool send(const MQMessage& msg) override;
    uint64_t get_sent_count() const override;
    uint64_t get_failed_count() const override;
    MessageQueueType get_type() const override {
        return MessageQueueType::RABBITMQ;
    }
    
private:
    Config config_;
    // amqpcpp 相关成员在实现文件中
};

// Pulsar 生产者
class PulsarProducerImpl : public MessageQueueProducer {
public:
    struct Config {
        std::string broker_url;  // "pulsar://localhost:6650"
        std::string version;  // "2.x", "3.x"
        std::string auth_token;
        std::map<std::string, std::string> properties;
    };
    
    explicit PulsarProducerImpl(const Config& config);
    ~PulsarProducerImpl();
    
    bool start() override;
    void stop() override;
    bool send(const MQMessage& msg) override;
    uint64_t get_sent_count() const override;
    uint64_t get_failed_count() const override;
    MessageQueueType get_type() const override {
        return MessageQueueType::PULSAR;
    }
    
private:
    Config config_;
    // pulsar 相关成员在实现文件中
};

// 消息队列生产者工厂
class MessageQueueProducerFactory {
public:
    static std::unique_ptr<MessageQueueProducer> create_kafka(
        const std::vector<std::string>& brokers,
        const std::string& version,
        const std::map<std::string, std::string>& properties = {});
    
    static std::unique_ptr<MessageQueueProducer> create_rabbitmq(
        const std::vector<std::string>& hosts,
        const std::string& version,
        const std::map<std::string, std::string>& properties = {});
    
    static std::unique_ptr<MessageQueueProducer> create_pulsar(
        const std::string& broker_url,
        const std::string& version,
        const std::map<std::string, std::string>& properties = {});
};

} // namespace logpipeline
