#include "router/tcp_server.h"
#include "router/client_session.h"
#include "common/message_queue.h"
#include <spdlog/spdlog.h>

namespace logpipeline {

RouterTCPServer::RouterTCPServer(const std::string& host, int port, int io_threads,
                                RouterMetricsAggregator* metrics_aggregator)    : host_(host),
      port_(port),
      io_threads_(io_threads),
      metrics_aggregator_(metrics_aggregator),
      running_(false) {
}

RouterTCPServer::~RouterTCPServer() {
    stop();
}

bool RouterTCPServer::start() {
    spdlog::info("Starting Router TCP server on {}:{} with {} IO threads",
                host_, port_, io_threads_);
    
    try {
        io_context_ = std::make_unique<asio::io_context>(io_threads_);
        
        asio::ip::tcp::endpoint endpoint(
            asio::ip::make_address(host_), port_);
        
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            *io_context_, endpoint);
        
        running_ = true;
        
        // 启动 IO 线程池
        for (int i = 0; i < io_threads_; ++i) {
            io_thread_pool_.emplace_back([this]() {
                io_context_->run();
            });
        }
        
        start_accept();
        
        spdlog::info("Router TCP server started successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start Router TCP server: {}", e.what());
        return false;
    }
}

void RouterTCPServer::stop() {
    if (!running_) {
        return;
    }
    
    spdlog::info("Stopping Router TCP server...");
    running_ = false;
    
    if (acceptor_) {
        acceptor_->close();
    }
    
    if (io_context_) {
        io_context_->stop();
    }
    
    for (auto& thread : io_thread_pool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    spdlog::info("Router TCP server stopped");
}

void RouterTCPServer::register_producer(const std::string& cluster_id,
                                       std::unique_ptr<MessageQueueProducer> producer) {
    std::lock_guard<std::mutex> lock(producers_mutex_);
    spdlog::info("Registering producer for cluster: {}", cluster_id);
    producers_[cluster_id] = std::move(producer);
    
    if (producers_[cluster_id]) {
        producers_[cluster_id]->start();
    }
}

MessageQueueProducer* RouterTCPServer::get_producer(const std::string& cluster_id) const {
    std::lock_guard<std::mutex> lock(producers_mutex_);
    auto it = producers_.find(cluster_id);
    if (it != producers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void RouterTCPServer::set_task_config(const TaskConfigMap& config) {
    std::lock_guard<std::mutex> lock(task_config_mutex_);
    task_config_map_ = config;
    spdlog::info("Updated task configuration with {} tasks", config.size());
}

void RouterTCPServer::start_accept() {
    if (!running_) {
        return;
    }
    
    // Lock and copy the config to pass to the session
    TaskConfigMap config_copy;
    {
        std::lock_guard<std::mutex> lock(task_config_mutex_);
        config_copy = task_config_map_;
    }

    auto session = std::make_shared<RouterClientSession>(
        asio::ip::tcp::socket(*io_context_),
        metrics_aggregator_,
        config_copy);
    
    acceptor_->async_accept(
        session->get_socket(),
        [this, session](const asio::error_code& error) {
            handle_accept(session, error);
        });
}

void RouterTCPServer::handle_accept(std::shared_ptr<RouterClientSession> session,
                                   const asio::error_code& error) {
    if (!running_) {
        return;
    }
    
    if (!error) {
        spdlog::info("Accepted new Agent connection from {}",
                    session->get_remote_address());
        
        // 设置生产者获取函数
        session->set_producer_getter([this](const std::string& cluster_id) {
            return get_producer(cluster_id);
        });
        
        active_connections_.fetch_add(1, std::memory_order_relaxed);
        
        session->start();
    } else {
        spdlog::error("Accept error: {}", error.message());
    }
    
    start_accept();
}

} // namespace logpipeline
