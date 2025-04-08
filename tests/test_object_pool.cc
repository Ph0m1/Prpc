#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include "object_pool.h"
#include "message_pool.h"

using namespace prpc;

// 测试用的简单对象
struct TestObject {
    int value;
    std::string data;
    
    TestObject() : value(0) {}
    
    void reset() {
        value = 0;
        data.clear();
    }
};

class ObjectPoolTest {
public:
    static void testBasicFunctionality() {
        std::cout << "Testing object pool basic functionality..." << std::endl;
        
        // 创建对象池
        ObjectPool<TestObject> pool(
            []() { return std::make_unique<TestObject>(); },
            [](TestObject* obj) { obj->reset(); },
            {5, 20, 60000, true, true}
        );
        
        // 测试获取对象
        auto obj1 = pool.acquire();
        assert(obj1);
        obj1->value = 42;
        obj1->data = "test";
        
        auto obj2 = pool.acquire();
        assert(obj2);
        assert(obj2->value == 0); // 应该是重置后的对象
        
        // 测试统计信息
        auto stats = pool.getStatistics();
        assert(stats.total_acquired.load() >= 2);
        
        std::cout << "Object pool basic functionality test passed!" << std::endl;
    }
    
    static void testConcurrency() {
        std::cout << "Testing object pool concurrency..." << std::endl;
        
        ObjectPool<TestObject> pool(
            []() { return std::make_unique<TestObject>(); },
            [](TestObject* obj) { obj->reset(); },
            {10, 50, 60000, true, true}
        );
        
        const int num_threads = 8;
        const int operations_per_thread = 100;
        std::atomic<int> success_count{0};
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&pool, &success_count, operations_per_thread]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    auto obj = pool.acquire(100); // 100ms超时
                    if (obj) {
                        obj->value = j;
                        obj->data = "thread_data_" + std::to_string(j);
                        success_count.fetch_add(1);
                        
                        // 模拟一些工作
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(success_count.load() > 0);
        
        auto stats = pool.getStatistics();
        std::cout << "Concurrency test - Success operations: " << success_count.load() 
                  << ", Total acquired: " << stats.total_acquired.load() << std::endl;
        
        std::cout << "Object pool concurrency test passed!" << std::endl;
    }
    
    static void testMessagePool() {
        std::cout << "Testing message pool..." << std::endl;
        
        auto& msg_pool = MessagePool::getInstance();
        
        // 测试消息获取
        auto msg = msg_pool.acquireMessage();
        assert(msg);
        
        msg->method_name = "test_method";
        msg->request_id = 12345;
        msg->payload = {1, 2, 3, 4, 5};
        
        // 测试缓冲区获取
        auto buffer = msg_pool.acquireBuffer();
        assert(buffer);
        
        buffer->resize(1024);
        buffer->write_pos = 512;
        buffer->read_pos = 100;
        
        assert(buffer->available() == 412);
        
        // 测试统计信息
        auto msg_stats = msg_pool.getMessageStats();
        auto buf_stats = msg_pool.getBufferStats();
        
        assert(msg_stats.total_acquired.load() >= 1);
        assert(buf_stats.total_acquired.load() >= 1);
        
        std::cout << "Message pool test passed!" << std::endl;
    }
    
    static void testPerformance() {
        std::cout << "Testing object pool performance..." << std::endl;
        
        ObjectPool<TestObject> pool(
            []() { return std::make_unique<TestObject>(); },
            [](TestObject* obj) { obj->reset(); },
            {50, 200, 60000, true, true}
        );
        
        const int iterations = 10000;
        
        // 测试对象池性能
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto obj = pool.acquire();
            if (obj) {
                obj->value = i;
                obj->data = "performance_test_" + std::to_string(i);
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        std::cout << "Object pool performance: " << ops_per_sec << " ops/sec" << std::endl;
        
        // 对比直接创建对象的性能
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto obj = std::make_unique<TestObject>();
            obj->value = i;
            obj->data = "direct_creation_" + std::to_string(i);
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double direct_ops_per_sec = (double)iterations / (duration.count() / 1000000.0);
        std::cout << "Direct creation performance: " << direct_ops_per_sec << " ops/sec" << std::endl;
        
        double improvement = (ops_per_sec / direct_ops_per_sec - 1.0) * 100.0;
        std::cout << "Performance improvement: " << improvement << "%" << std::endl;
        
        std::cout << "Object pool performance test passed!" << std::endl;
    }
    
    static void testMemoryUsage() {
        std::cout << "Testing object pool memory usage..." << std::endl;
        
        ObjectPool<TestObject> pool(
            []() { return std::make_unique<TestObject>(); },
            [](TestObject* obj) { obj->reset(); },
            {10, 50, 5000, true, true} // 5秒空闲时间，便于测试
        );
        
        // 获取大量对象然后释放
        std::vector<ObjectPool<TestObject>::PooledObject> objects;
        
        for (int i = 0; i < 30; ++i) {
            auto obj = pool.acquire();
            if (obj) {
                objects.push_back(std::move(obj));
            }
        }
        
        auto stats_before = pool.getStatistics();
        std::cout << "Before release - Active: " << stats_before.active_objects.load() 
                  << ", Pool size: " << stats_before.current_size.load() << std::endl;
        
        // 释放所有对象
        objects.clear();
        
        auto stats_after = pool.getStatistics();
        std::cout << "After release - Active: " << stats_after.active_objects.load() 
                  << ", Pool size: " << stats_after.current_size.load() << std::endl;
        
        assert(stats_after.active_objects.load() == 0);
        
        std::cout << "Object pool memory usage test passed!" << std::endl;
    }
    
    static void testMessagePoolPerformance() {
        std::cout << "Testing message pool performance..." << std::endl;
        
        auto& msg_pool = MessagePool::getInstance();
        const int iterations = 5000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto msg = msg_pool.acquireMessage();
            if (msg) {
                msg->method_name = "benchmark_method_" + std::to_string(i);
                msg->request_id = i;
                msg->payload.resize(1024);
                std::fill(msg->payload.begin(), msg->payload.end(), i % 256);
            }
            
            auto buffer = msg_pool.acquireBuffer();
            if (buffer) {
                buffer->resize(2048);
                buffer->write_pos = 1024;
                buffer->read_pos = 0;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double ops_per_sec = (double)(iterations * 2) / (duration.count() / 1000000.0);
        std::cout << "Message pool performance: " << ops_per_sec << " ops/sec" << std::endl;
        
        // 打印统计信息
        msg_pool.printStatistics();
        
        std::cout << "Message pool performance test passed!" << std::endl;
    }
    
    static void runAllTests() {
        std::cout << "=========================================" << std::endl;
        std::cout << "           Object Pool Tests" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        testBasicFunctionality();
        std::cout << std::endl;
        
        testConcurrency();
        std::cout << std::endl;
        
        testMessagePool();
        std::cout << std::endl;
        
        testPerformance();
        std::cout << std::endl;
        
        testMemoryUsage();
        std::cout << std::endl;
        
        testMessagePoolPerformance();
        std::cout << std::endl;
        
        std::cout << "All object pool tests passed!" << std::endl;
    }
};

int main(int argc, char** argv) {
    try {
        ObjectPoolTest::runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
} 