#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <asio.hpp>

namespace logpipeline {

class RouterMetricsAggregator;

// Router Prometheus 指标端点
// 暴露 HTTP 接口供监控系统采集 Prometheus 格式的指标
class RouterMetricsEndpoint {
public:
    RouterMetricsEndpoint(const std::string& listen_address,
                         RouterMetricsAggregator* aggregator);
    ~RouterMetricsEndpoint();
    
    bool start();
    void stop();
    
private:
    std::string listen_address_;
    std::string host_;
    int port_;
    RouterMetricsAggregator* aggregator_;
    
    std::atomic<bool> running_;
    std::unique_ptr<asio::io_context> io_context_;
    std::thread io_thread_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    
    void start_accept();
    void handle_accept(std::shared_ptr<asio::ip::tcp::socket> socket,
                      const asio::error_code& error);
    
    class HttpRequestHandler;
};

} // namespace logpipeline
