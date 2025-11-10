#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <librdkafka/rdkafkacpp.h>

namespace logpipeline {

struct KafkaMessage {
    std::string topic;
    std::string key;
    std::string payload;
    uint64_t task_id;
    uint64_t offset;
    std::function<void(bool)> callback;
};

class KafkaProducer {
public:
    KafkaProducer(const std::vector<std::string>& bootstrap_servers,
                  const std::map<std::string, std::string>& default_props);
    ~KafkaProducer();
    
    bool start();
    void stop();
    
    bool send_message(const KafkaMessage& message);
    
    uint64_t get_messages_sent() const { return messages_sent_.load(); }
    uint64_t get_messages_failed() const { return messages_failed_.load(); }
    
private:
    class DeliveryReportCallback;
    
    std::vector<std::string> bootstrap_servers_;
    std::map<std::string, std::string> default_props_;
    
    std::atomic<bool> running_;
    std::unique_ptr<RdKafka::Producer> producer_;
    std::unique_ptr<RdKafka::Topic> default_topic_;
    
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> messages_failed_{0};
    
    // Message queue
    std::queue<KafkaMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread producer_thread_;
    
    void producer_loop();
    void handle_delivery_report(RdKafka::Message& message);
    
    std::unique_ptr<RdKafka::Conf> create_config() const;
};

} // namespace logpipeline
