#include <iostream>
#include <cassert>
#include <chrono>
#include <vector>
#include <random>
#include <string>
#include <fstream>
#include <thread>
#include <atomic>
#include <future>
#include "logger.h"
#include "threadpool.h"
#include "network_utils.h"
#include "conf.h"
#include "object_pool.h"
#include "message_pool.h"
#include "pool_monitor.h"

using namespace prpc;

// 简单的基准测试框架
class SimpleBenchmark {
public:
    static void benchmarkLoggerPerformance() {
        std::cout << "Benchmarking logger performance..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        const int iterations = 10000;
        
        for (int i = 0; i < iterations; ++i) {
            LOG(INFO) << "Benchmark test message " << i;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        std::cout << "Logger performance: " << ops_per_sec << " ops/sec" << std::endl;
    }
    
    static void benchmarkThreadPoolPerformance() {
        std::cout << "Benchmarking thread pool performance..." << std::endl;
        
        ThreadPool pool(4);
        std::atomic<int> counter{0};
        const int iterations = 1000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<int>> futures;
        for (int i = 0; i < iterations; ++i) {
            futures.push_back(pool.submit([&counter]() {
                counter.fetch_add(1);
                return counter.load();
            }));
        }
        
        // 等待所有任务完成
        for (auto& future : futures) {
            future.get();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        std::cout << "Thread pool performance: " << ops_per_sec << " ops/sec" << std::endl;
    }
    
    static void benchmarkConfigReadPerformance() {
        std::cout << "Benchmarking config read performance..." << std::endl;
        
        // 创建临时配置文件
        std::ofstream config_file("benchmark_test.conf");
        for (int i = 0; i < 1000; ++i) {
            config_file << "key" << i << "=value" << i << std::endl;
        }
        config_file.close();
        
        const int iterations = 100;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            Pconfig config;
            config.LoadConfigFile("benchmark_test.conf");
            std::string value = config.Load("key500");
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // 清理
        std::remove("benchmark_test.conf");
        
        double ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        std::cout << "Config read performance: " << ops_per_sec << " ops/sec" << std::endl;
    }
    
    static void benchmarkMemoryAllocation() {
        std::cout << "Benchmarking memory allocation..." << std::endl;
        
        const int iterations = 10000;
        const size_t size = 1024;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            std::vector<char> buffer(size);
            // 防止编译器优化掉
            volatile char* ptr = buffer.data();
            (void)ptr;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        double mb_per_sec = (ops_per_sec * size) / (1024 * 1024);
        std::cout << "Memory allocation performance: " << ops_per_sec << " ops/sec, " 
                  << mb_per_sec << " MB/sec" << std::endl;
    }
    
    static void benchmarkStringOperations() {
        std::cout << "Benchmarking string operations..." << std::endl;
        
        std::vector<std::string> strings;
        strings.reserve(1000);
        
        // 准备测试数据
        for (int i = 0; i < 1000; ++i) {
            strings.push_back("test_string_" + std::to_string(i));
        }
        
        const int iterations = 1000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            std::string result;
            for (const auto& str : strings) {
                result += str + "_";
            }
            // 防止编译器优化掉
            volatile size_t len = result.length();
            (void)len;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        std::cout << "String operations performance: " << ops_per_sec << " ops/sec" << std::endl;
    }
    
    static void benchmarkObjectPool() {
        std::cout << "Benchmarking object pool..." << std::endl;
        
        // 测试简单对象池
        struct TestObj { int value; std::string data; };
        ObjectPool<TestObj> pool(
            []() { return std::make_unique<TestObj>(); },
            [](TestObj* obj) { obj->value = 0; obj->data.clear(); },
            {50, 200, 60000, true, true}
        );
        
        const int iterations = 50000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto obj = pool.acquire();
            if (obj) {
                obj->value = i;
                obj->data = "test_" + std::to_string(i);
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        std::cout << "Object pool performance: " << ops_per_sec << " ops/sec" << std::endl;
        
        auto stats = pool.getStatistics();
        double hit_rate = (double)stats.cache_hits.load() / 
                         (stats.cache_hits.load() + stats.cache_misses.load()) * 100.0;
        std::cout << "Object pool hit rate: " << hit_rate << "%" << std::endl;
    }
    
    static void benchmarkMessagePool() {
        std::cout << "Benchmarking message pool..." << std::endl;
        
        auto& msg_pool = MessagePool::getInstance();
        const int iterations = 20000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto msg = msg_pool.acquireMessage();
            if (msg) {
                msg->method_name = "benchmark_method_" + std::to_string(i);
                msg->request_id = i;
                msg->payload.resize(512);
                std::fill(msg->payload.begin(), msg->payload.end(), i % 256);
            }
            
            auto buffer = msg_pool.acquireBuffer();
            if (buffer) {
                buffer->resize(1024);
                buffer->write_pos = 512;
                buffer->read_pos = 0;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)(iterations * 2) / (duration.count() / 1000000.0);
        std::cout << "Message pool performance: " << ops_per_sec << " ops/sec" << std::endl;
        
        // 显示统计信息
        auto msg_stats = msg_pool.getMessageStats();
        auto buf_stats = msg_pool.getBufferStats();
        
        auto msg_total = msg_stats.cache_hits.load() + msg_stats.cache_misses.load();
        auto buf_total = buf_stats.cache_hits.load() + buf_stats.cache_misses.load();
        
        if (msg_total > 0) {
            double msg_hit_rate = (double)msg_stats.cache_hits.load() / msg_total * 100.0;
            std::cout << "Message pool hit rate: " << msg_hit_rate << "%" << std::endl;
        }
        
        if (buf_total > 0) {
            double buf_hit_rate = (double)buf_stats.cache_hits.load() / buf_total * 100.0;
            std::cout << "Buffer pool hit rate: " << buf_hit_rate << "%" << std::endl;
        }
    }
    
    static void benchmarkPoolMonitor() {
        std::cout << "Testing pool monitor..." << std::endl;
        
        auto& monitor = PoolMonitor::getInstance();
        
        // 生成一些池活动
        auto& msg_pool = MessagePool::getInstance();
        for (int i = 0; i < 1000; ++i) {
            auto msg = msg_pool.acquireMessage();
            auto buffer = msg_pool.acquireBuffer();
        }
        
        // 生成报告
        auto report = monitor.generateReport();
        std::cout << "Pool monitor report generated successfully" << std::endl;
        
        // 检查健康状态
        auto health = monitor.checkHealth();
        std::cout << "Pool health status: " << (health.is_healthy ? "HEALTHY" : "WARNING") << std::endl;
        
        // 获取性能指标
        auto metrics = monitor.getPerformanceMetrics();
        std::cout << "Message pool efficiency: " << (metrics.message_pool_efficiency * 100) << "%" << std::endl;
        std::cout << "Buffer pool efficiency: " << (metrics.buffer_pool_efficiency * 100) << "%" << std::endl;
    }
    
    static void runAllBenchmarks() {
        std::cout << "=========================================" << std::endl;
        std::cout << "           Performance Benchmarks" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        benchmarkLoggerPerformance();
        std::cout << std::endl;
        
        benchmarkThreadPoolPerformance();
        std::cout << std::endl;
        
        benchmarkConfigReadPerformance();
        std::cout << std::endl;
        
        benchmarkMemoryAllocation();
        std::cout << std::endl;
        
        benchmarkStringOperations();
        std::cout << std::endl;
        
        benchmarkObjectPool();
        std::cout << std::endl;
        
        benchmarkMessagePool();
        std::cout << std::endl;
        
        benchmarkPoolMonitor();
        std::cout << std::endl;
        
        std::cout << "All benchmarks completed!" << std::endl;
    }
};

int main(int argc, char** argv) {
    try {
        SimpleBenchmark::runAllBenchmarks();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Benchmark failed with unknown exception" << std::endl;
        return 1;
    }
} 