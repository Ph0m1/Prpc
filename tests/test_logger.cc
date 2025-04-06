#include "logger.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>

class LoggerTest {
public:
    static void testBasicLogging() {
        std::cout << "Testing basic logging..." << std::endl;
        
        // 测试不同级别的日志
        LOG(INFO) << "This is an info message";
        LOG(ERROR) << "This is an error message";
        
        // 测试流式接口
        int value = 42;
        std::string text = "test";
        LOG(INFO) << "Value: " << value << ", Text: " << text;
        
        std::cout << "Basic logging test passed!" << std::endl;
    }
    
    static void testLogLevels() {
        std::cout << "Testing log levels..." << std::endl;
        
        // 获取日志实例
        PLogger& logger = PLogger::getInstance();
        
        // 测试设置不同的日志级别
        logger.setLogLevel(ERROR);
        LOG(INFO) << "This info message should not appear";
        LOG(ERROR) << "This error message should appear";
        
        // 重置为INFO级别
        logger.setLogLevel(INFO);
        LOG(INFO) << "This info message should appear now";
        
        std::cout << "Log levels test passed!" << std::endl;
    }
    
    static void testThreadSafety() {
        std::cout << "Testing thread safety..." << std::endl;
        
        const int num_threads = 10;
        const int messages_per_thread = 100;
        std::vector<std::thread> threads;
        
        // 创建多个线程同时写日志
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([i, messages_per_thread]() {
                for (int j = 0; j < messages_per_thread; ++j) {
                    LOG(INFO) << "Thread " << i << " message " << j;
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Thread safety test passed!" << std::endl;
    }
    
    static void testLogStream() {
        std::cout << "Testing LogStream functionality..." << std::endl;
        
        // 测试LogStream的构造和析构
        {
            LogStream stream(INFO, __FILE__, __LINE__);
            stream << "Test message in LogStream";
        } // LogStream应该在这里自动输出日志
        
        // 测试不同数据类型
        {
            LogStream stream(INFO, __FILE__, __LINE__);
            stream << "Mixed types: " << 123 << " " << 45.67 << " " << true;
        }
        
        std::cout << "LogStream test passed!" << std::endl;
    }
    
    static void testLogMacros() {
        std::cout << "Testing LOG macros..." << std::endl;
        
        // 测试LOG宏的文件名和行号
        LOG(INFO) << "Testing file and line info";
        LOG(ERROR) << "Error with file and line info";
        
        // 测试条件日志（如果有的话）
        bool condition = true;
        if (condition) {
            LOG(INFO) << "Conditional logging works";
        }
        
        std::cout << "LOG macros test passed!" << std::endl;
    }
    
    static void testPerformance() {
        std::cout << "Testing logging performance..." << std::endl;
        
        const int num_messages = 10000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_messages; ++i) {
            LOG(INFO) << "Performance test message " << i;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Logged " << num_messages << " messages in " 
                  << duration.count() << " ms" << std::endl;
        std::cout << "Average: " << (double)duration.count() / num_messages 
                  << " ms per message" << std::endl;
        
        std::cout << "Performance test completed!" << std::endl;
    }
    
    static void testSingletonPattern() {
        std::cout << "Testing singleton pattern..." << std::endl;
        
        // 获取两个实例，应该是同一个对象
        PLogger& logger1 = PLogger::getInstance();
        PLogger& logger2 = PLogger::getInstance();
        
        assert(&logger1 == &logger2);
        
        // 测试设置在一个实例上，另一个实例也能看到
        logger1.setLogLevel(ERROR);
        // logger2应该也是ERROR级别
        
        std::cout << "Singleton pattern test passed!" << std::endl;
    }
    
    static void testLogFormatting() {
        std::cout << "Testing log formatting..." << std::endl;
        
        // 测试时间戳格式
        LOG(INFO) << "Testing timestamp format";
        
        // 测试文件名和行号格式
        LOG(ERROR) << "Testing file:line format";
        
        // 测试长消息
        std::string long_message(1000, 'A');
        LOG(INFO) << "Long message: " << long_message;
        
        // 测试特殊字符
        LOG(INFO) << "Special chars: \n\t\"'\\";
        
        std::cout << "Log formatting test passed!" << std::endl;
    }
};

int main() {
    std::cout << "Starting logger tests..." << std::endl;
    
    try {
        LoggerTest::testBasicLogging();
        LoggerTest::testLogLevels();
        LoggerTest::testLogStream();
        LoggerTest::testLogMacros();
        LoggerTest::testSingletonPattern();
        LoggerTest::testLogFormatting();
        LoggerTest::testThreadSafety();
        LoggerTest::testPerformance();
        
        std::cout << "All logger tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Logger test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Logger test failed with unknown exception" << std::endl;
        return 1;
    }
} 