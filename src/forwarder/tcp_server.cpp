#include "forwarder/tcp_server.h"
#include "forwarder/client_session.h"
#include <spdlog/spdlog.h>

namespace logpipeline {

TCPServer::TCPServer(const std::string& host, int port, int io_threads,
                     KafkaProducer* kafka_producer, MetricsAggregator* metrics_aggregator)
    : host_(host), port_(port), io_threads_(io_threads),
      kafka_producer_(kafka_producer), metrics_aggregator_(metrics_aggregator),
      running_(false) {
}

TCPServer::~TCPServer() {
    stop();
}

bool TCPServer::start() {
    if (running_) {
        return true;
    }
    
    try {
        io_context_ = std::make_unique<asio::io_context>();
        
        // Create acceptor
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host_), port_);
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(*io_context_, endpoint);
        
        running_ = true;
        
        // Start IO threads
        for (int i = 0; i < io_threads_; ++i) {
            io_threads_.emplace_back([this]() {
                try {
                    io_context_->run();
                } catch (const std::exception& e) {
                    spdlog::error("IO thread error: {}", e.what());
                }
            });
        }
        
        // Start accepting connections
        start_accept();
        
        spdlog::info("TCPServer started on {}:{}", host_, port_);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to start TCPServer: {}", e.what());
        return false;
    }
}

void TCPServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (io_context_) {
        io_context_->stop();
    }
    
    if (acceptor_ && acceptor_->is_open()) {
        asio::error_code ec;
        acceptor_->close(ec);
    }
    
    // Wait for IO threads to finish
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    io_threads_.clear();
    
    spdlog::info("TCPServer stopped");
}

void TCPServer::start_accept() {
    if (!running_) {
        return;
    }
    
    auto session = std::make_shared<ClientSession>(
        asio::ip::tcp::socket(*io_context_),
        kafka_producer_,
        metrics_aggregator_
    );
    
    acceptor_->async_accept(session->get_socket(),
        [this, session](const asio::error_code& error) {
            if (!error) {
                active_connections_++;
                spdlog::info("New connection from: {}", session->get_remote_address());
                
                session->start();
                
                // Update metrics
                if (metrics_aggregator_) {
                    metrics_aggregator_->set_active_connections(active_connections_.load());
                }
            } else {
                spdlog::error("Accept error: {}", error.message());
            }
            
            // Continue accepting
            start_accept();
        });
}

void TCPServer::handle_accept(std::shared_ptr<ClientSession> session,
                              const asio::error_code& error) {
    if (!error) {
        active_connections_++;
        spdlog::info("New connection from: {}", session->get_remote_address());
        
        session->start();
        
        // Update metrics
        if (metrics_aggregator_) {
            metrics_aggregator_->set_active_connections(active_connections_.load());
        }
    } else {
        spdlog::error("Accept error: {}", error.message());
    }
    
    // Continue accepting
    start_accept();
}

} // namespace logpipeline
