#include "threadpool.h"
#include "logger.h"
#include "error.h"
#include "network_utils.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <atomic>
#include <thread>
#include <random>

class PerformanceTest {
public:
    static void testThreadPoolPerformance() {
        std::cout << "Testing thread pool performance..." << std::endl;
        
        const int num_tasks = 100000;
        const int num_threads = std::thread::hardware_concurrency();
        
        ThreadPool pool(num_threads);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<int>> futures;
        futures.reserve(num_tasks);
        
        // 提交大量轻量级任务
        for (int i = 0; i < num_tasks; ++i) {
            futures.push_back(pool.submit([i]() {
                return i % 1000;
            }));
        }
        
        // 等待所有任务完成
        for (auto& future : futures) {
            future.get();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "ThreadPool Performance:" << std::endl;
        std::cout << "  Tasks: " << num_tasks << std::endl;
        std::cout << "  Threads: " << num_threads << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Throughput: " << (num_tasks * 1000.0 / duration.count()) << " tasks/sec" << std::endl;
        
        std::cout << "Thread pool performance test completed!" << std::endl;
    }
    
    static void testLoggingPerformance() {
        std::cout << "Testing logging performance..." << std::endl;
        
        const int num_messages = 50000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_messages; ++i) {
            LOG(INFO) << "Performance test message " << i << " with some additional data";
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Logging Performance:" << std::endl;
        std::cout << "  Messages: " << num_messages << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Throughput: " << (num_messages * 1000.0 / duration.count()) << " messages/sec" << std::endl;
        std::cout << "  Average: " << (double)duration.count() / num_messages << " ms/message" << std::endl;
        
        std::cout << "Logging performance test completed!" << std::endl;
    }
    
    static void testErrorHandlingPerformance() {
        std::cout << "Testing error handling performance..." << std::endl;
        
        const int num_operations = 100000;
        
        // 测试成功情况的性能
        auto start_success = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_operations; ++i) {
            auto result = prpc::ErrorHandler::safeExecute([i]() -> int {
                return i * 2;
            });
            (void)result.getValue(); // 使用结果避免优化
        }
        
        auto end_success = std::chrono::high_resolution_clock::now();
        auto duration_success = std::chrono::duration_cast<std::chrono::microseconds>(end_success - start_success);
        
        // 测试异常情况的性能
        auto start_error = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) { // 减少异常测试次数
            auto result = prpc::ErrorHandler::safeExecute([]() -> int {
                throw prpc::ServiceException("Test exception");
            });
            (void)result.getErrorCode(); // 使用结果避免优化
        }
        
        auto end_error = std::chrono::high_resolution_clock::now();
        auto duration_error = std::chrono::duration_cast<std::chrono::microseconds>(end_error - start_error);
        
        std::cout << "Error Handling Performance:" << std::endl;
        std::cout << "  Success cases: " << num_operations << " operations in " 
                  << duration_success.count() << " μs" << std::endl;
        std::cout << "  Success throughput: " << (num_operations * 1000000.0 / duration_success.count()) 
                  << " ops/sec" << std::endl;
        std::cout << "  Error cases: 1000 operations in " << duration_error.count() << " μs" << std::endl;
        std::cout << "  Error throughput: " << (1000 * 1000000.0 / duration_error.count()) 
                  << " ops/sec" << std::endl;
        
        std::cout << "Error handling performance test completed!" << std::endl;
    }
    
    static void testConcurrentLogging() {
        std::cout << "Testing concurrent logging performance..." << std::endl;
        
        const int num_threads = 8;
        const int messages_per_thread = 10000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([t, messages_per_thread]() {
                for (int i = 0; i < messages_per_thread; ++i) {
                    LOG(INFO) << "Thread " << t << " message " << i;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        int total_messages = num_threads * messages_per_thread;
        
        std::cout << "Concurrent Logging Performance:" << std::endl;
        std::cout << "  Threads: " << num_threads << std::endl;
        std::cout << "  Total messages: " << total_messages << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Throughput: " << (total_messages * 1000.0 / duration.count()) << " messages/sec" << std::endl;
        
        std::cout << "Concurrent logging performance test completed!" << std::endl;
    }
    
    static void testMemoryUsage() {
        std::cout << "Testing memory usage..." << std::endl;
        
        // 测试大量对象创建和销毁
        const int num_iterations = 10000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_iterations; ++i) {
            // 创建和销毁Result对象
            prpc::Result<std::string> result("Test string " + std::to_string(i));
            (void)result.getValue(); // 使用对象避免优化
            
            // 创建和销毁异常对象
            try {
                throw prpc::NetworkException("Test exception " + std::to_string(i));
            } catch (const prpc::PrpcException& e) {
                (void)e.what(); // 使用异常避免优化
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Memory Usage Test:" << std::endl;
        std::cout << "  Iterations: " << num_iterations << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Average: " << (double)duration.count() / num_iterations << " ms/iteration" << std::endl;
        
        std::cout << "Memory usage test completed!" << std::endl;
    }
    
    static void testNetworkPerformance() {
        std::cout << "Testing network performance..." << std::endl;
        
        try {
            const int num_sockets = 100;
            
            auto start = std::chrono::high_resolution_clock::now();
            
            std::vector<prpc::network::Socket> sockets;
            sockets.reserve(num_sockets);
            
            // 创建大量socket
            for (int i = 0; i < num_sockets; ++i) {
                sockets.emplace_back(AF_INET, SOCK_STREAM);
                sockets.back().setReuseAddr();
                sockets.back().setKeepAlive();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "Network Performance:" << std::endl;
            std::cout << "  Sockets created: " << num_sockets << std::endl;
            std::cout << "  Time: " << duration.count() << " ms" << std::endl;
            std::cout << "  Average: " << (double)duration.count() / num_sockets << " ms/socket" << std::endl;
            
        } catch (const prpc::NetworkException& e) {
            std::cout << "Network performance test failed: " << e.what() << std::endl;
        }
        
        std::cout << "Network performance test completed!" << std::endl;
    }
    
    static void testScalabilityTest() {
        std::cout << "Testing scalability..." << std::endl;
        
        std::vector<int> thread_counts = {1, 2, 4, 8, 16};
        const int tasks_per_thread = 10000;
        
        for (int num_threads : thread_counts) {
            ThreadPool pool(num_threads);
            
            auto start = std::chrono::high_resolution_clock::now();
            
            std::vector<std::future<int>> futures;
            int total_tasks = num_threads * tasks_per_thread;
            futures.reserve(total_tasks);
            
            for (int i = 0; i < total_tasks; ++i) {
                futures.push_back(pool.submit([i]() {
                    // 模拟一些计算工作
                    int sum = 0;
                    for (int j = 0; j < 100; ++j) {
                        sum += j;
                    }
                    return sum + i;
                }));
            }
            
            for (auto& future : futures) {
                future.get();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "  Threads: " << num_threads 
                      << ", Tasks: " << total_tasks
                      << ", Time: " << duration.count() << " ms"
                      << ", Throughput: " << (total_tasks * 1000.0 / duration.count()) << " tasks/sec"
                      << std::endl;
        }
        
        std::cout << "Scalability test completed!" << std::endl;
    }
    
    static void testStressTest() {
        std::cout << "Testing stress conditions..." << std::endl;
        
        const int stress_duration_seconds = 10;
        const int num_threads = std::thread::hardware_concurrency() * 2;
        
        std::atomic<bool> stop_flag(false);
        std::atomic<long> operations_completed(0);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&stop_flag, &operations_completed, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(1, 100);
                
                while (!stop_flag.load()) {
                    // 随机选择操作类型
                    int op_type = dis(gen) % 4;
                    
                    switch (op_type) {
                        case 0: // 日志记录
                            LOG(INFO) << "Stress test thread " << t << " logging";
                            break;
                        case 1: // 错误处理
                            {
                                auto result = prpc::ErrorHandler::safeExecute([]() -> int {
                                    return 42;
                                });
                                (void)result.getValue();
                            }
                            break;
                        case 2: // 异常处理
                            try {
                                throw prpc::ServiceException("Stress test exception");
                            } catch (const prpc::PrpcException& e) {
                                (void)e.what();
                            }
                            break;
                        case 3: // 网络对象创建
                            try {
                                prpc::network::Socket socket(AF_INET, SOCK_STREAM);
                                (void)socket.isValid();
                            } catch (const prpc::NetworkException& e) {
                                (void)e.what();
                            }
                            break;
                        default:
                            // 不应该到达这里
                            break;
                    }
                    
                    operations_completed.fetch_add(1);
                }
            });
        }
        
        // 运行指定时间
        std::this_thread::sleep_for(std::chrono::seconds(stress_duration_seconds));
        stop_flag.store(true);
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Stress Test Results:" << std::endl;
        std::cout << "  Duration: " << duration.count() << " ms" << std::endl;
        std::cout << "  Threads: " << num_threads << std::endl;
        std::cout << "  Operations: " << operations_completed.load() << std::endl;
        std::cout << "  Throughput: " << (operations_completed.load() * 1000.0 / duration.count()) 
                  << " ops/sec" << std::endl;
        
        std::cout << "Stress test completed!" << std::endl;
    }
};

int main() {
    std::cout << "Starting performance tests..." << std::endl;
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads" << std::endl;
    std::cout << std::endl;
    
    try {
        PerformanceTest::testThreadPoolPerformance();
        std::cout << std::endl;
        
        PerformanceTest::testLoggingPerformance();
        std::cout << std::endl;
        
        PerformanceTest::testErrorHandlingPerformance();
        std::cout << std::endl;
        
        PerformanceTest::testConcurrentLogging();
        std::cout << std::endl;
        
        PerformanceTest::testMemoryUsage();
        std::cout << std::endl;
        
        PerformanceTest::testNetworkPerformance();
        std::cout << std::endl;
        
        PerformanceTest::testScalabilityTest();
        std::cout << std::endl;
        
        PerformanceTest::testStressTest();
        std::cout << std::endl;
        
        std::cout << "All performance tests completed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Performance test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Performance test failed with unknown exception" << std::endl;
        return 1;
    }
} 