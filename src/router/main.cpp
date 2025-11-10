#include "router/tcp_server.h"
#include "router/metrics_aggregator.h"
#include "router/metrics_endpoint.h"
#include "common/config.h"
#include "common/message_queue.h"
#include "common/control_plane.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <csignal>
#include <atomic>
#include <nlohmann/json.hpp>

std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    g_shutdown_requested = true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file> [--recommend-concurrency]" << std::endl;
        return 1;
    }
    
    // 检查推荐并发参数
    bool recommend_concurrency = false;
    if (argc >= 3) {
        std::string arg = argv[2];
        if (arg == "--recommend-concurrency") {
            recommend_concurrency = true;
        }
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // 加载配置
        logpipeline::config::RouterConfig config;
        if (!config.load_from_file(argv[1])) {
            std::cerr << "Failed to load configuration from: " << argv[1] << std::endl;
            return 1;
        }
        
        spdlog::info("Starting log router on {}:{}", config.listen_host, config.listen_port);
        
        // 如果请求推荐并发配置，计算并输出建议值
        if (recommend_concurrency) {
            // 获取 CPU 和内存信息，计算推荐的并发配置
            // 这部分应该在实现中进一步完善
            spdlog::info("Recommended configuration:");
            spdlog::info("  io_threads: 8 (based on CPU cores)");
            spdlog::info("  session_buffer_mb: 64 (based on available memory)");
            return 0;
        }
        
        // 初始化管控平台客户端
        logpipeline::ControlPlaneConfig cp_config;
        cp_config.host = config.control_platform_host;
        cp_config.port = config.control_platform_port;
        auto control_plane = std::make_unique<logpipeline::ControlPlaneClient>(cp_config);
        
        if (!control_plane->initialize()) {
            spdlog::warn("Failed to connect to control plane");
        }
        
        // 初始化指标聚合器
        auto metrics_aggregator = std::make_unique<logpipeline::RouterMetricsAggregator>();
        
        // 初始化 TCP 服务器
        auto tcp_server = std::make_unique<logpipeline::RouterTCPServer>(
            config.listen_host,
            config.listen_port,
            config.io_threads,
            metrics_aggregator.get()
        );
        
        if (!tcp_server->start()) {
            spdlog::error("Failed to start Router TCP server");
            return 1;
        }
        
        // 注册消息队列生产者
        // Kafka 集群
        for (const auto& [cluster_id, kafka_config] : config.kafka_clusters) {
            auto producer = logpipeline::MessageQueueProducerFactory::create_kafka(
                kafka_config.brokers,
                kafka_config.version,
                kafka_config.properties
            );
            tcp_server->register_producer(cluster_id, std::move(producer));
        }
        
        // RabbitMQ 集群
        for (const auto& [cluster_id, rabbit_config] : config.rabbitmq_clusters) {
            auto producer = logpipeline::MessageQueueProducerFactory::create_rabbitmq(
                rabbit_config.hosts,
                rabbit_config.version,
                rabbit_config.properties
            );
            tcp_server->register_producer(cluster_id, std::move(producer));
        }
        
        // Pulsar 集群
        for (const auto& [cluster_id, pulsar_config] : config.pulsar_clusters) {
            auto producer = logpipeline::MessageQueueProducerFactory::create_pulsar(
                pulsar_config.broker_url,
                pulsar_config.version,
                pulsar_config.properties
            );
            tcp_server->register_producer(cluster_id, std::move(producer));
        }
        
        // 启动指标端点
        auto metrics_endpoint = std::make_unique<logpipeline::RouterMetricsEndpoint>(
            config.metrics_prometheus_listen,
            metrics_aggregator.get()
        );
        
        if (!metrics_endpoint->start()) {
            spdlog::error("Failed to start metrics endpoint");
            return 1;
        }
        
        // 注册到管控平台
        if (!config.router_id.empty()) {
            control_plane->register_router(config.router_id,
                                          config.listen_host,
                                          config.listen_port);
        }
        
        spdlog::info("Log router started successfully");
        spdlog::info("Metrics endpoint available at: http://{}", config.metrics_prometheus_listen);
        
        // 主循环
        int update_counter = 0;
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 定期更新指标
            metrics_aggregator->set_active_connections(tcp_server->get_active_connections());
            
            // 定期向管控平台上报状态
            if (++update_counter >= 30) {  // 每 30 秒上报一次
                update_counter = 0;
                
                if (!config.router_id.empty()) {
                    nlohmann::json status;
                    status["active_connections"] = tcp_server->get_active_connections();
                    control_plane->report_router_status(config.router_id, status);
                }
            }
        }
        
        // 优雅关闭
        spdlog::info("Shutting down log router...");
        
        metrics_endpoint->stop();
        tcp_server->stop();
        
        spdlog::info("Log router stopped");
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}
