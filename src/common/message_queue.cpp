#include "common/message_queue.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

// Kafka 相关头文件（如果可用）
#ifdef HAVE_LIBRDKAFKA
#include <librdkafka/rdkafkacpp.h>
#endif

namespace logpipeline {

// ==================== Kafka Producer 实现 ====================

class KafkaProducerImpl::Impl {
public:
    Impl() = default;
    ~Impl() {
        if (running_) {
            stop();
        }
    }

    void stop() {
        running_ = false;
        queue_cv_.notify_all();
        if (producer_thread_.joinable()) {
            producer_thread_.join();
        }
    }

#ifdef HAVE_LIBRDKAFKA
    std::unique_ptr<RdKafka::Producer> producer_;
    std::map<std::string, std::unique_ptr<RdKafka::Topic>> topics_;
#endif
    std::atomic<uint64_t> sent_count_{0};
    std::atomic<uint64_t> failed_count_{0};
    
    std::queue<MQMessage> msg_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread producer_thread_;
    std::atomic<bool> running_{false};
};

KafkaProducerImpl::KafkaProducerImpl(const Config& config)
    : impl_(std::make_unique<Impl>()), config_(config) {
}

KafkaProducerImpl::~KafkaProducerImpl() {
    stop();
}

bool KafkaProducerImpl::start() {
#ifdef HAVE_LIBRDKAFKA
    spdlog::info("Starting Kafka producer, version: {}", config_.version);
    
    std::string error_str;
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    
    if (!conf) {
        spdlog::error("Failed to create Kafka config");
        return false;
    }
    
    // 设置 broker 列表
    std::string brokers_str;
    for (size_t i = 0; i < config_.brokers.size(); ++i) {
        if (i > 0) brokers_str += ",";
        brokers_str += config_.brokers[i];
    }
    
    if (conf->set("bootstrap.servers", brokers_str, error_str) !=
        RdKafka::Conf::CONF_OK) {
        spdlog::error("Failed to set bootstrap.servers: {}", error_str);
        delete conf;
        return false;
    }
    
    // 设置其他属性
    for (const auto& [key, value] : config_.properties) {
        if (conf->set(key, value, error_str) != RdKafka::Conf::CONF_OK) {
            spdlog::warn("Failed to set property {}={}: {}", key, value, error_str);
        }
    }
    
    // 创建生产者
    impl_->producer_.reset(RdKafka::Producer::create(conf, error_str));
    if (!impl_->producer_) {
        spdlog::error("Failed to create Kafka producer: {}", error_str);
        delete conf;
        return false;
    }
    
    delete conf;
    
    impl_->running_ = true;
    // In a real implementation, a thread would poll the producer and handle delivery reports.
    // This is simplified for now.
    
    spdlog::info("Kafka producer started successfully");
    return true;
#else
    spdlog::warn("Kafka support not compiled");
    return false;
#endif
}

void KafkaProducerImpl::stop() {
    if (impl_->running_.exchange(false)) {
        spdlog::info("Stopping Kafka producer");
        impl_->stop();
    }
}

bool KafkaProducerImpl::send(const MQMessage& msg) {
#ifdef HAVE_LIBRDKAFKA
    if (!impl_->running_) return false;

    spdlog::debug("Sending message to Kafka topic: {}", msg.topic);
    
    RdKafka::ErrorCode err = impl_->producer_->produce(
        msg.topic,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<void*>(static_cast<const void*>(msg.payload.data())),
        msg.payload.size(),
        msg.key.c_str(),
        msg.key.length(),
        0, /* timestamp */
        nullptr /* msg_opaque */);

    if (err != RdKafka::ERR_NO_ERROR) {
        spdlog::error("Failed to produce to topic {}: {}", msg.topic, RdKafka::err2str(err));
        impl_->failed_count_++;
        return false;
    }

    impl_->sent_count_++;
    impl_->producer_->poll(0);
    return true;
#else
    return false;
#endif
}

uint64_t KafkaProducerImpl::get_sent_count() const {
    return impl_->sent_count_.load();
}

uint64_t KafkaProducerImpl::get_failed_count() const {
    return impl_->failed_count_.load();
}

// ==================== RabbitMQ Producer 实现 ====================

RabbitMQProducerImpl::RabbitMQProducerImpl(const Config& config)
    : config_(config) {
}

RabbitMQProducerImpl::~RabbitMQProducerImpl() {
    stop();
}

bool RabbitMQProducerImpl::start() {
    spdlog::info("Starting RabbitMQ producer, version: {}", config_.version);
    // 实现 RabbitMQ 连接逻辑
    spdlog::warn("RabbitMQ support is being implemented");
    return true;
}

void RabbitMQProducerImpl::stop() {
    spdlog::info("Stopping RabbitMQ producer");
}

bool RabbitMQProducerImpl::send(const MQMessage& msg) {
    spdlog::debug("Sending message to RabbitMQ exchange: {}", msg.topic);
    return true;
}

uint64_t RabbitMQProducerImpl::get_sent_count() const {
    return 0;
}

uint64_t RabbitMQProducerImpl::get_failed_count() const {
    return 0;
}

// ==================== Pulsar Producer 实现 ====================

PulsarProducerImpl::PulsarProducerImpl(const Config& config)
    : config_(config) {
}

PulsarProducerImpl::~PulsarProducerImpl() {
    stop();
}

bool PulsarProducerImpl::start() {
    spdlog::info("Starting Pulsar producer, version: {}", config_.version);
    // 实现 Pulsar 连接逻辑
    spdlog::warn("Pulsar support is being implemented");
    return true;
}

void PulsarProducerImpl::stop() {
    spdlog::info("Stopping Pulsar producer");
}

bool PulsarProducerImpl::send(const MQMessage& msg) {
    spdlog::debug("Sending message to Pulsar topic: {}", msg.topic);
    return true;
}

uint64_t PulsarProducerImpl::get_sent_count() const {
    return 0;
}

uint64_t PulsarProducerImpl::get_failed_count() const {
    return 0;
}

// ==================== 工厂类实现 ====================

std::unique_ptr<MessageQueueProducer> MessageQueueProducerFactory::create_kafka(
    const std::vector<std::string>& brokers,
    const std::string& version,
    const std::map<std::string, std::string>& properties) {
    
    auto config = std::make_shared<KafkaProducerImpl::Config>();
    config->brokers = brokers;
    config->version = version;
    config->properties = properties;
    
    return std::make_unique<KafkaProducerImpl>(*config);
}

std::unique_ptr<MessageQueueProducer> MessageQueueProducerFactory::create_rabbitmq(
    const std::vector<std::string>& hosts,
    const std::string& version,
    const std::map<std::string, std::string>& properties) {
    
    auto config = std::make_shared<RabbitMQProducerImpl::Config>();
    config->hosts = hosts;
    config->version = version;
    config->properties = properties;
    
    return std::make_unique<RabbitMQProducerImpl>(*config);
}

std::unique_ptr<MessageQueueProducer> MessageQueueProducerFactory::create_pulsar(
    const std::string& broker_url,
    const std::string& version,
    const std::map<std::string, std::string>& properties) {
    
    auto config = std::make_shared<PulsarProducerImpl::Config>();
    config->broker_url = broker_url;
    config->version = version;
    config->properties = properties;
    
    return std::make_unique<PulsarProducerImpl>(*config);
}

} // namespace logpipeline
