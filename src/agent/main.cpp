#include "agent/zk_manager.h"
#include "agent/task_manager.h"
#include "agent/checkpoint_manager.h"
#include "agent/metrics_collector.h"
#include "agent/tcp_client.h"
#include "common/config.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <csignal>
#include <atomic>

#include <spdlog/sinks/stdout_color_sinks.h>

std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    g_shutdown_requested = true;
}

int main(int argc, char* argv[]) {
    // Setup logging
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::debug);

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // Load configuration
        logpipeline::config::AgentConfig config;
        if (!config.load_from_file(argv[1])) {
            std::cerr << "Failed to load configuration from: " << argv[1] << std::endl;
            return 1;
        }
        
        spdlog::info("Starting log agent with ID: {}", config.agent_id);
        
        // Initialize components
        auto checkpoint_manager = std::make_unique<logpipeline::CheckpointManager>(config.checkpoint_file_path);
        if (!checkpoint_manager->load()) {
            spdlog::warn("Failed to load checkpoints, starting fresh");
        }
        
        auto tcp_client = std::make_unique<logpipeline::AgentTCPClient>(config.forwarders);
        if (!tcp_client->start()) {
            spdlog::error("Failed to start TCP client");
            return 1;
        }
        
        auto metrics_collector = std::make_unique<logpipeline::MetricsCollector>(tcp_client.get());
        metrics_collector->start(config.metrics_report_interval_sec);
        
        auto task_manager = std::make_unique<logpipeline::TaskManager>(tcp_client.get(), metrics_collector.get());
        task_manager->start();
        
        auto zk_manager = std::make_unique<logpipeline::ZKManager>(
            config.zookeeper_hosts,
            config.agent_status_path,
            config.task_config_path,
            config.agent_id
        );
        
        zk_manager->set_task_manager(task_manager.get());
        
        if (!zk_manager->start()) {
            spdlog::error("Failed to start ZooKeeper manager");
            return 1;
        }
        
        spdlog::info("Log agent started successfully");
        
        // Main loop
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Periodically save checkpoints
            static int checkpoint_counter = 0;
            if (++checkpoint_counter >= 60) { // Save every minute
                checkpoint_manager->save();
                checkpoint_counter = 0;
            }
        }
        
        // Graceful shutdown
        spdlog::info("Shutting down log agent...");
        
        zk_manager->stop();
        task_manager->stop();
        metrics_collector->stop();
        tcp_client->stop();
        
        // Final checkpoint save
        checkpoint_manager->save();
        
        spdlog::info("Log agent stopped");
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}
