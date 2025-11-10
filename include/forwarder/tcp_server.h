#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <asio.hpp>

namespace logpipeline {

class ClientSession;
class KafkaProducer;
class MetricsAggregator;

class TCPServer {
public:
    TCPServer(const std::string& host, int port, int io_threads,
              KafkaProducer* kafka_producer, MetricsAggregator* metrics_aggregator);
    ~TCPServer();
    
    bool start();
    void stop();
    
    int get_active_connections() const { return active_connections_.load(); }
    
private:
    std::string host_;
    int port_;
    int io_threads_;
    
    KafkaProducer* kafka_producer_;
    MetricsAggregator* metrics_aggregator_;
    
    std::atomic<bool> running_;
    std::unique_ptr<asio::io_context> io_context_;
    std::vector<std::thread> io_threads_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    
    std::atomic<int> active_connections_{0};
    
    void start_accept();
    void handle_accept(std::shared_ptr<ClientSession> session,
                      const asio::error_code& error);
};

} // namespace logpipeline
