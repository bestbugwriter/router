#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <asio.hpp>
#include <map>

namespace logpipeline {

class RouterClientSession;
class MessageQueueProducer;
class MetricsAggregator;

// Router TCP 服务器
// 接收 Agent 发来的压缩日志数据
// 分发到不同的消息队列集群
class RouterTCPServer {
public:
    RouterTCPServer(const std::string& host, int port, int io_threads,
                    MetricsAggregator* metrics_aggregator);
    ~RouterTCPServer();
    
    // 启动/停止服务器
    bool start();
    void stop();
    
    // 获取活跃连接数
    int get_active_connections() const { 
        return active_connections_.load(); 
    }
    
    // 注册消息队列生产者
    void register_producer(const std::string& cluster_id,
                          std::unique_ptr<MessageQueueProducer> producer);
    
    // 获取指定集群的生产者
    MessageQueueProducer* get_producer(const std::string& cluster_id) const;
    
private:
    std::string host_;
    int port_;
    int io_threads_;
    
    MetricsAggregator* metrics_aggregator_;
    
    // 消息队列生产者映射 (cluster_id -> producer)
    std::map<std::string, std::unique_ptr<MessageQueueProducer>> producers_;
    mutable std::mutex producers_mutex_;
    
    std::atomic<bool> running_;
    std::unique_ptr<asio::io_context> io_context_;
    std::vector<std::thread> io_thread_pool_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    
    std::atomic<int> active_connections_{0};
    
    void start_accept();
    void handle_accept(std::shared_ptr<RouterClientSession> session,
                      const asio::error_code& error);
};

} // namespace logpipeline
