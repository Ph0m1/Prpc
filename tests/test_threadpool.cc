#include "threadpool.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

class ThreadPoolTest {
public:
    static void testBasicSubmit() {
        std::cout << "Testing basic task submission..." << std::endl;
        
        ThreadPool pool(4);
        
        // 提交一个简单任务
        auto future = pool.submit([]() {
            return 42;
        });
        
        assert(future.get() == 42);
        
        std::cout << "Basic submit test passed!" << std::endl;
    }
    
    static void testMultipleTasks() {
        std::cout << "Testing multiple tasks..." << std::endl;
        
        ThreadPool pool(4);
        std::vector<std::future<int>> futures;
        
        // 提交多个任务
        for (int i = 0; i < 10; ++i) {
            futures.push_back(pool.submit([i]() {
                return i * i;
            }));
        }
        
        // 验证结果
        for (int i = 0; i < 10; ++i) {
            assert(futures[i].get() == i * i);
        }
        
        std::cout << "Multiple tasks test passed!" << std::endl;
    }
    
    static void testTaskWithParameters() {
        std::cout << "Testing tasks with parameters..." << std::endl;
        
        ThreadPool pool(2);
        
        // 测试带参数的任务
        auto future1 = pool.submit([](int a, int b) {
            return a + b;
        }, 10, 20);
        
        auto future2 = pool.submit([](const std::string& str, int count) {
            std::string result;
            for (int i = 0; i < count; ++i) {
                result += str;
            }
            return result;
        }, "Hello", 3);
        
        assert(future1.get() == 30);
        assert(future2.get() == "HelloHelloHello");
        
        std::cout << "Tasks with parameters test passed!" << std::endl;
    }
    
    static void testConcurrentExecution() {
        std::cout << "Testing concurrent execution..." << std::endl;
        
        ThreadPool pool(4);
        std::atomic<int> counter(0);
        std::vector<std::future<void>> futures;
        
        // 提交多个会修改共享变量的任务
        for (int i = 0; i < 100; ++i) {
            futures.push_back(pool.submit([&counter]() {
                for (int j = 0; j < 1000; ++j) {
                    counter.fetch_add(1);
                }
            }));
        }
        
        // 等待所有任务完成
        for (auto& future : futures) {
            future.get();
        }
        
        assert(counter.load() == 100000);
        
        std::cout << "Concurrent execution test passed!" << std::endl;
    }
    
    static void testTaskExecution() {
        std::cout << "Testing task execution order..." << std::endl;
        
        ThreadPool pool(1); // 单线程池，确保顺序执行
        std::vector<int> results;
        std::mutex results_mutex;
        std::vector<std::future<void>> futures;
        
        // 提交有序任务
        for (int i = 0; i < 5; ++i) {
            futures.push_back(pool.submit([i, &results, &results_mutex]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                std::lock_guard<std::mutex> lock(results_mutex);
                results.push_back(i);
            }));
        }
        
        // 等待所有任务完成
        for (auto& future : futures) {
            future.get();
        }
        
        // 验证执行顺序（单线程池应该按提交顺序执行）
        assert(results.size() == 5);
        for (int i = 0; i < 5; ++i) {
            assert(results[i] == i);
        }
        
        std::cout << "Task execution order test passed!" << std::endl;
    }
    
    static void testExceptionHandling() {
        std::cout << "Testing exception handling..." << std::endl;
        
        ThreadPool pool(2);
        
        // 提交会抛出异常的任务
        auto future = pool.submit([]() -> int {
            throw std::runtime_error("Test exception");
        });
        
        try {
            future.get();
            assert(false); // 不应该到达这里
        } catch (const std::runtime_error& e) {
            assert(std::string(e.what()) == "Test exception");
        }
        
        // 确保线程池仍然可以处理其他任务
        auto future2 = pool.submit([]() {
            return 100;
        });
        
        assert(future2.get() == 100);
        
        std::cout << "Exception handling test passed!" << std::endl;
    }
    
    static void testThreadPoolDestruction() {
        std::cout << "Testing thread pool destruction..." << std::endl;
        
        std::atomic<int> completed_tasks(0);
        
        {
            ThreadPool pool(4);
            std::vector<std::future<void>> futures;
            
            // 提交一些长时间运行的任务
            for (int i = 0; i < 10; ++i) {
                futures.push_back(pool.submit([&completed_tasks]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    completed_tasks.fetch_add(1);
                }));
            }
            
            // 等待一些任务完成
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } // ThreadPool析构函数应该等待所有任务完成
        
        // 验证所有任务都已完成
        assert(completed_tasks.load() == 10);
        
        std::cout << "Thread pool destruction test passed!" << std::endl;
    }
    
    static void testPerformance() {
        std::cout << "Testing performance..." << std::endl;
        
        const int num_tasks = 10000;
        const int num_threads = std::thread::hardware_concurrency();
        
        ThreadPool pool(num_threads);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<int>> futures;
        for (int i = 0; i < num_tasks; ++i) {
            futures.push_back(pool.submit([i]() {
                // 模拟一些计算工作
                int sum = 0;
                for (int j = 0; j < 1000; ++j) {
                    sum += j;
                }
                return sum + i;
            }));
        }
        
        // 等待所有任务完成
        for (auto& future : futures) {
            future.get();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Completed " << num_tasks << " tasks in " 
                  << duration.count() << " ms" << std::endl;
        std::cout << "Average: " << (double)duration.count() / num_tasks 
                  << " ms per task" << std::endl;
        
        std::cout << "Performance test completed!" << std::endl;
    }
    
    static void testDifferentReturnTypes() {
        std::cout << "Testing different return types..." << std::endl;
        
        ThreadPool pool(2);
        
        // 测试void返回类型
        auto future_void = pool.submit([]() {
            // 无返回值任务
        });
        future_void.get();
        
        // 测试string返回类型
        auto future_string = pool.submit([]() -> std::string {
            return "Hello World";
        });
        assert(future_string.get() == "Hello World");
        
        // 测试vector返回类型
        auto future_vector = pool.submit([]() -> std::vector<int> {
            return {1, 2, 3, 4, 5};
        });
        auto result = future_vector.get();
        assert(result.size() == 5);
        assert(result[0] == 1 && result[4] == 5);
        
        std::cout << "Different return types test passed!" << std::endl;
    }
};

int main() {
    std::cout << "Starting thread pool tests..." << std::endl;
    
    try {
        ThreadPoolTest::testBasicSubmit();
        ThreadPoolTest::testMultipleTasks();
        ThreadPoolTest::testTaskWithParameters();
        ThreadPoolTest::testConcurrentExecution();
        ThreadPoolTest::testTaskExecution();
        ThreadPoolTest::testExceptionHandling();
        ThreadPoolTest::testDifferentReturnTypes();
        ThreadPoolTest::testPerformance();
        ThreadPoolTest::testThreadPoolDestruction();
        
        std::cout << "All thread pool tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Thread pool test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Thread pool test failed with unknown exception" << std::endl;
        return 1;
    }
} 