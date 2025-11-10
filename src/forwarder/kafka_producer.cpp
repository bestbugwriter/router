#include "forwarder/kafka_producer.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace logpipeline {

class KafkaProducer::DeliveryReportCallback : public RdKafka::DeliveryReportCb {
public:
    explicit DeliveryReportCallback(KafkaProducer* producer) : producer_(producer) {}
    
    void dr_cb(RdKafka::Message& message) override {
        producer_->handle_delivery_report(message);
    }
    
private:
    KafkaProducer* producer_;
};

KafkaProducer::KafkaProducer(const std::vector<std::string>& bootstrap_servers,
                             const std::map<std::string, std::string>& default_props)
    : bootstrap_servers_(bootstrap_servers), default_props_(default_props), running_(false) {
}

KafkaProducer::~KafkaProducer() {
    stop();
}

bool KafkaProducer::start() {
    if (running_) {
        return true;
    }
    
    try {
        // Create configuration
        auto conf = create_config();
        if (!conf) {
            return false;
        }
        
        // Set delivery report callback
        auto dr_callback = std::make_unique<DeliveryReportCallback>(this);
        conf->set("dr_cb", dr_callback.get(), nullptr);
        
        // Create producer
        std::string errstr;
        producer_.reset(RdKafka::Producer::create(conf.get(), errstr));
        if (!producer_) {
            spdlog::error("Failed to create Kafka producer: {}", errstr);
            return false;
        }
        
        running_ = true;
        producer_thread_ = std::thread(&KafkaProducer::producer_loop, this);
        
        spdlog::info("KafkaProducer started");
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception starting Kafka producer: {}", e.what());
        return false;
    }
}

void KafkaProducer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Signal producer thread to stop
    queue_cv_.notify_all();
    
    if (producer_thread_.joinable()) {
        producer_thread_.join();
    }
    
    // Flush producer before destroying
    if (producer_) {
        producer_->flush(5000); // 5 second timeout
    }
    
    spdlog::info("KafkaProducer stopped");
}

bool KafkaProducer::send_message(const KafkaMessage& message) {
    if (!running_ || !producer_) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push(message);
    }
    
    queue_cv_.notify_one();
    return true;
}

void KafkaProducer::producer_loop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait(lock, [this]() {
            return !message_queue_.empty() || !running_;
        });
        
        if (!running_) {
            break;
        }
        
        while (!message_queue_.empty()) {
            auto message = std::move(message_queue_.front());
            message_queue_.pop();
            lock.unlock();
            
            // Send to Kafka
            RdKafka::ErrorCode err = producer_->produce(
                message.topic,
                RdKafka::Topic::PARTITION_UA,
                RdKafka::Producer::RK_MSG_COPY,
                const_cast<char*>(message.payload.c_str()),
                message.payload.length(),
                message.key.empty() ? nullptr : message.key.c_str(),
                message.key.length(),
                0, // timestamp
                nullptr, // headers
                nullptr // opaque
            );
            
            if (err != RdKafka::ERR_NO_ERROR) {
                spdlog::error("Kafka produce error: {}", RdKafka::err2str(err));
                messages_failed_++;
                
                // Call callback with failure
                if (message.callback) {
                    message.callback(false);
                }
            } else {
                // Success will be reported through delivery callback
                // We'll increment counter there
            }
            
            lock.lock();
        }
    }
}

void KafkaProducer::handle_delivery_report(RdKafka::Message& message) {
    if (message.err() == RdKafka::ERR_NO_ERROR) {
        messages_sent_++;
        spdlog::debug("Message delivered to topic {} [{}] offset {}", 
                     message.topic_name(), message.partition(), message.offset());
    } else {
        messages_failed_++;
        spdlog::error("Message delivery failed: {}", message.errstr());
    }
}

std::unique_ptr<RdKafka::Conf> KafkaProducer::create_config() const {
    auto conf = std::unique_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    
    if (!conf) {
        spdlog::error("Failed to create Kafka configuration");
        return nullptr;
    }
    
    // Set bootstrap servers
    std::string bootstrap_servers_str;
    for (size_t i = 0; i < bootstrap_servers_.size(); ++i) {
        if (i > 0) bootstrap_servers_str += ",";
        bootstrap_servers_str += bootstrap_servers_[i];
    }
    
    std::string errstr;
    if (conf->set("bootstrap.servers", bootstrap_servers_str, errstr) != RdKafka::Conf::CONF_OK) {
        spdlog::error("Failed to set bootstrap.servers: {}", errstr);
        return nullptr;
    }
    
    // Set default properties
    for (const auto& [key, value] : default_props_) {
        if (conf->set(key, value, errstr) != RdKafka::Conf::CONF_OK) {
            spdlog::warn("Failed to set Kafka property {}: {}", key, errstr);
        }
    }
    
    // Set some reasonable defaults
    conf->set("queue.buffering.max.ms", "500", errstr);
    conf->set("compression.codec", "snappy", errstr);
    conf->set("client.id", "log-forwarder", errstr);
    
    return conf;
}

} // namespace logpipeline
