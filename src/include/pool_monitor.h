#ifndef POOL_MONITOR_H
#define POOL_MONITOR_H

#include "object_pool.h"
#include "message_pool.h"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace prpc {

/**
 * @brief 对象池监控器
 */
class PoolMonitor {
public:
    struct MonitorConfig {
        uint32_t report_interval_seconds;       // 报告间隔(秒)
        bool enable_file_logging;               // 启用文件日志
        bool enable_console_output;             // 启用控制台输出
        std::string log_file_path;
        bool enable_alerts;                     // 启用告警
        double high_usage_threshold;            // 高使用率阈值
        double low_hit_rate_threshold;          // 低命中率阈值
        
        MonitorConfig() 
            : report_interval_seconds(60)
            , enable_file_logging(true)
            , enable_console_output(false)
            , log_file_path("pool_monitor.log")
            , enable_alerts(true)
            , high_usage_threshold(0.8)
            , low_hit_rate_threshold(0.5) {}
    };
    
    static PoolMonitor& getInstance() {
        static PoolMonitor instance;
        return instance;
    }
    
    /**
     * @brief 启动监控
     */
    void start(const MonitorConfig& config = MonitorConfig{}) {
        if (running_.load()) {
            return;
        }
        
        config_ = config;
        running_.store(true);
        
        monitor_thread_ = std::thread(&PoolMonitor::monitorLoop, this);
    }
    
    /**
     * @brief 停止监控
     */
    void stop() {
        running_.store(false);
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
    
    /**
     * @brief 生成即时报告
     */
    std::string generateReport() const {
        auto& msg_pool = MessagePool::getInstance();
        auto msg_stats = msg_pool.getMessageStats();
        auto buf_stats = msg_pool.getBufferStats();
        
        std::ostringstream oss;
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        oss << "=== Pool Monitor Report ===\n";
        oss << "Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";
        
        // 消息池统计
        oss << "Message Pool:\n";
        oss << "  Total Created: " << msg_stats.total_created.load() << "\n";
        oss << "  Total Acquired: " << msg_stats.total_acquired.load() << "\n";
        oss << "  Total Returned: " << msg_stats.total_returned.load() << "\n";
        oss << "  Current Size: " << msg_stats.current_size.load() << "\n";
        oss << "  Active Objects: " << msg_stats.active_objects.load() << "\n";
        
        auto msg_total = msg_stats.cache_hits.load() + msg_stats.cache_misses.load();
        if (msg_total > 0) {
            double hit_rate = (double)msg_stats.cache_hits.load() / msg_total;
            oss << "  Hit Rate: " << std::fixed << std::setprecision(2) << (hit_rate * 100) << "%\n";
        }
        
        // 缓冲区池统计
        oss << "\nBuffer Pool:\n";
        oss << "  Total Created: " << buf_stats.total_created.load() << "\n";
        oss << "  Total Acquired: " << buf_stats.total_acquired.load() << "\n";
        oss << "  Total Returned: " << buf_stats.total_returned.load() << "\n";
        oss << "  Current Size: " << buf_stats.current_size.load() << "\n";
        oss << "  Active Objects: " << buf_stats.active_objects.load() << "\n";
        
        auto buf_total = buf_stats.cache_hits.load() + buf_stats.cache_misses.load();
        if (buf_total > 0) {
            double hit_rate = (double)buf_stats.cache_hits.load() / buf_total;
            oss << "  Hit Rate: " << std::fixed << std::setprecision(2) << (hit_rate * 100) << "%\n";
        }
        
        // 健康状态检查
        oss << "\nHealth Status:\n";
        auto health = checkHealth();
        oss << "  Overall Status: " << (health.is_healthy ? "HEALTHY" : "WARNING") << "\n";
        
        if (!health.warnings.empty()) {
            oss << "  Warnings:\n";
            for (const auto& warning : health.warnings) {
                oss << "    - " << warning << "\n";
            }
        }
        
        return oss.str();
    }
    
    /**
     * @brief 健康状态结构
     */
    struct HealthStatus {
        bool is_healthy = true;
        std::vector<std::string> warnings;
    };
    
    /**
     * @brief 检查池健康状态
     */
    HealthStatus checkHealth() const {
        HealthStatus health;
        auto& msg_pool = MessagePool::getInstance();
        auto msg_stats = msg_pool.getMessageStats();
        auto buf_stats = msg_pool.getBufferStats();
        
        // 检查消息池使用率
        auto msg_total_capacity = msg_stats.current_size.load() + msg_stats.active_objects.load();
        if (msg_total_capacity > 0) {
            double msg_usage = (double)msg_stats.active_objects.load() / msg_total_capacity;
            if (msg_usage > config_.high_usage_threshold) {
                health.is_healthy = false;
                health.warnings.push_back("Message pool high usage: " + 
                    std::to_string(int(msg_usage * 100)) + "%");
            }
        }
        
        // 检查缓冲区池使用率
        auto buf_total_capacity = buf_stats.current_size.load() + buf_stats.active_objects.load();
        if (buf_total_capacity > 0) {
            double buf_usage = (double)buf_stats.active_objects.load() / buf_total_capacity;
            if (buf_usage > config_.high_usage_threshold) {
                health.is_healthy = false;
                health.warnings.push_back("Buffer pool high usage: " + 
                    std::to_string(int(buf_usage * 100)) + "%");
            }
        }
        
        // 检查命中率
        auto msg_total = msg_stats.cache_hits.load() + msg_stats.cache_misses.load();
        if (msg_total > 100) { // 至少有100次操作才检查命中率
            double hit_rate = (double)msg_stats.cache_hits.load() / msg_total;
            if (hit_rate < config_.low_hit_rate_threshold) {
                health.is_healthy = false;
                health.warnings.push_back("Message pool low hit rate: " + 
                    std::to_string(int(hit_rate * 100)) + "%");
            }
        }
        
        auto buf_total = buf_stats.cache_hits.load() + buf_stats.cache_misses.load();
        if (buf_total > 100) {
            double hit_rate = (double)buf_stats.cache_hits.load() / buf_total;
            if (hit_rate < config_.low_hit_rate_threshold) {
                health.is_healthy = false;
                health.warnings.push_back("Buffer pool low hit rate: " + 
                    std::to_string(int(hit_rate * 100)) + "%");
            }
        }
        
        return health;
    }
    
    /**
     * @brief 获取性能指标
     */
    struct PerformanceMetrics {
        double message_pool_efficiency;    // 消息池效率
        double buffer_pool_efficiency;     // 缓冲区池效率
        uint64_t total_operations;         // 总操作数
        double operations_per_second;      // 每秒操作数
    };
    
    PerformanceMetrics getPerformanceMetrics() const {
        auto& msg_pool = MessagePool::getInstance();
        auto msg_stats = msg_pool.getMessageStats();
        auto buf_stats = msg_pool.getBufferStats();
        
        PerformanceMetrics metrics;
        
        // 计算效率(命中率)
        auto msg_total = msg_stats.cache_hits.load() + msg_stats.cache_misses.load();
        metrics.message_pool_efficiency = msg_total > 0 ? 
            (double)msg_stats.cache_hits.load() / msg_total : 0.0;
        
        auto buf_total = buf_stats.cache_hits.load() + buf_stats.cache_misses.load();
        metrics.buffer_pool_efficiency = buf_total > 0 ? 
            (double)buf_stats.cache_hits.load() / buf_total : 0.0;
        
        metrics.total_operations = msg_total + buf_total;
        
        // 计算每秒操作数(基于启动时间)
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        metrics.operations_per_second = duration.count() > 0 ? 
            (double)metrics.total_operations / duration.count() : 0.0;
        
        return metrics;
    }

private:
    PoolMonitor() : running_(false), start_time_(std::chrono::steady_clock::now()) {}
    
    ~PoolMonitor() {
        stop();
    }
    
    // 禁用拷贝和移动
    PoolMonitor(const PoolMonitor&) = delete;
    PoolMonitor& operator=(const PoolMonitor&) = delete;
    
    /**
     * @brief 监控循环
     */
    void monitorLoop() {
        while (running_.load()) {
            try {
                auto report = generateReport();
                
                // 输出到控制台
                if (config_.enable_console_output) {
                    std::cout << report << std::endl;
                }
                
                // 写入文件
                if (config_.enable_file_logging) {
                    writeToFile(report);
                }
                
                // 检查告警
                if (config_.enable_alerts) {
                    checkAndSendAlerts();
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Pool monitor error: " << e.what() << std::endl;
            }
            
            // 等待下一个报告间隔
            std::this_thread::sleep_for(std::chrono::seconds(config_.report_interval_seconds));
        }
    }
    
    /**
     * @brief 写入文件
     */
    void writeToFile(const std::string& report) {
        std::ofstream file(config_.log_file_path, std::ios::app);
        if (file.is_open()) {
            file << report << "\n" << std::string(50, '=') << "\n\n";
            file.close();
        }
    }
    
    /**
     * @brief 检查并发送告警
     */
    void checkAndSendAlerts() {
        auto health = checkHealth();
        if (!health.is_healthy) {
            for (const auto& warning : health.warnings) {
                std::cerr << "[POOL ALERT] " << warning << std::endl;
                // 这里可以集成更复杂的告警系统，如发送邮件、Slack通知等
            }
        }
    }

private:
    std::atomic<bool> running_;
    std::thread monitor_thread_;
    MonitorConfig config_;
    std::chrono::steady_clock::time_point start_time_;
};

/**
 * @brief 便利函数：启动池监控
 */
inline void startPoolMonitoring(const PoolMonitor::MonitorConfig& config = PoolMonitor::MonitorConfig{}) {
    PoolMonitor::getInstance().start(config);
}

/**
 * @brief 便利函数：停止池监控
 */
inline void stopPoolMonitoring() {
    PoolMonitor::getInstance().stop();
}

/**
 * @brief 便利函数：获取池报告
 */
inline std::string getPoolReport() {
    return PoolMonitor::getInstance().generateReport();
}

} // namespace prpc

#endif // POOL_MONITOR_H 