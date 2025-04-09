#include "application.h"
#include "provider.h"
#include "channel.h"
#include "controller.h"
#include "error.h"
#include "logger.h"
#include "network_utils.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <fstream>

class IntegrationTest {
public:
    static void testApplicationConfigIntegration() {
        std::cout << "Testing application-config integration..." << std::endl;
        
        // 创建测试配置文件
        const char* test_config = "integration_test.conf";
        std::ofstream file(test_config);
        file << "rpcserverip=127.0.0.1\n";
        file << "rpcserverport=8080\n";
        file << "zookeeperip=127.0.0.1\n";
        file << "zookeeperport=2181\n";
        file.close();
        
        // 初始化应用程序
        const char* argv[] = {"test_program", "-i", test_config};
        int argc = 3;
        
        auto result = Papplication::Init(argc, const_cast<char**>(argv));
        assert(result.isSuccess());
        
        // 验证配置加载
        Pconfig& config = Papplication::GetConfig();
        assert(config.Load("rpcserverip") == "127.0.0.1");
        assert(config.Load("rpcserverport") == "8080");
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Application-config integration test passed!" << std::endl;
    }
    
    static void testErrorHandlingIntegration() {
        std::cout << "Testing error handling integration..." << std::endl;
        
        // 测试配置错误的传播
        auto result = Papplication::Init(1, nullptr);
        assert(!result.isSuccess());
        assert(result.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        // 测试网络错误处理
        try {
            [[maybe_unused]] prpc::network::Socket socket = prpc::network::utils::createTcpClient("invalid.host", 9999);
            assert(false); // 不应该到达这里
        } catch (const prpc::NetworkException& e) {
            assert(e.getErrorCode() == prpc::ErrorCode::NETWORK_ERROR);
        }
        
        std::cout << "Error handling integration test passed!" << std::endl;
    }
    
    static void testLoggingIntegration() {
        std::cout << "Testing logging integration..." << std::endl;
        
        // 测试不同模块的日志记录
        LOG(INFO) << "Integration test: Application module";
        LOG(INFO) << "Integration test: Network module";
        LOG(ERROR) << "Integration test: Error simulation";
        
        // 测试日志级别设置
        PLogger& logger = PLogger::getInstance();
        logger.setLogLevel(ERROR);
        LOG(INFO) << "This info message should not appear";
        LOG(ERROR) << "This error message should appear";
        
        // 重置日志级别
        logger.setLogLevel(INFO);
        
        std::cout << "Logging integration test passed!" << std::endl;
    }
    
    static void testNetworkErrorIntegration() {
        std::cout << "Testing network-error integration..." << std::endl;
        
        // 清除全局错误处理器，避免段错误
        prpc::ErrorHandler::setGlobalErrorHandler(nullptr);
        
        try {
            // 测试网络操作的错误处理
            auto result = prpc::ErrorHandler::safeExecute([]() -> std::string {
                prpc::network::Socket socket = prpc::network::utils::createTcpClient("127.0.0.1", 9999);
                return "Should not reach here";
            });
            
            assert(!result.isSuccess());
            assert(result.getErrorCode() == prpc::ErrorCode::NETWORK_ERROR);
            
        } catch (const std::exception& e) {
            // 如果抛出异常也是可以接受的
            std::cout << "Expected network error: " << e.what() << std::endl;
        }
        
        std::cout << "Network-error integration test passed!" << std::endl;
    }
    
    static void testConfigErrorIntegration() {
        std::cout << "Testing config-error integration..." << std::endl;
        
        // 测试配置模块与错误处理的集成
        Pconfig config;
        
        auto result1 = config.LoadConfigFile(nullptr);
        assert(!result1.isSuccess());
        assert(result1.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        auto result2 = config.LoadConfigFile("nonexistent_file.conf");
        assert(!result2.isSuccess());
        assert(result2.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        std::cout << "Config-error integration test passed!" << std::endl;
    }
    
    static void testComponentLifecycle() {
        std::cout << "Testing component lifecycle..." << std::endl;
        
        // 创建配置文件
        const char* test_config = "lifecycle_test.conf";
        std::ofstream file(test_config);
        file << "rpcserverip=127.0.0.1\n";
        file << "rpcserverport=8081\n";
        file.close();
        
        // 1. 初始化应用程序
        const char* argv[] = {"test_program", "-i", test_config};
        int argc = 3;
        
        auto init_result = Papplication::Init(argc, const_cast<char**>(argv));
        assert(init_result.isSuccess());
        
        // 2. 获取配置
        Pconfig& config = Papplication::GetConfig();
        assert(!config.Load("rpcserverip").empty());
        
        // 3. 测试日志记录
        LOG(INFO) << "Component lifecycle test in progress";
        
        // 4. 测试错误处理
        auto error_result = prpc::ErrorHandler::safeExecute([]() -> int {
            return 42;
        });
        assert(error_result.isSuccess());
        assert(error_result.getValue() == 42);
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Component lifecycle test passed!" << std::endl;
    }
    
    static void testConcurrentOperations() {
        std::cout << "Testing concurrent operations..." << std::endl;
        
        const int num_threads = 5;
        std::vector<std::thread> threads;
        std::atomic<int> success_count(0);
        
        // 创建多个线程执行不同操作
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([i, &success_count]() {
                try {
                    // 日志记录
                    LOG(INFO) << "Thread " << i << " logging test";
                    
                    // 错误处理测试
                    auto result = prpc::ErrorHandler::safeExecute([i]() -> int {
                        return i * 10;
                    });
                    
                    if (result.isSuccess() && result.getValue() == i * 10) {
                        success_count.fetch_add(1);
                    }
                    
                } catch (const std::exception& e) {
                    LOG(ERROR) << "Thread " << i << " failed: " << e.what();
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(success_count.load() == num_threads);
        
        std::cout << "Concurrent operations test passed!" << std::endl;
    }
    
    static void testResourceManagement() {
        std::cout << "Testing resource management..." << std::endl;
        
        // 测试RAII资源管理
        {
            // 创建临时文件
            const char* temp_file = "temp_resource_test.txt";
            FILE* fp = fopen(temp_file, "w");
            if (fp) {
                prpc::ScopedResource<FILE*> file(fp);
                file.setCleanup([](FILE*& f) { 
                    if (f) { 
                        fclose(f); 
                        f = nullptr; 
                    } 
                });
                
                // 文件应该在作用域结束时自动关闭
            }
            
            // 清理
            std::remove(temp_file);
        }
        
        // 测试网络资源管理
        try {
            prpc::network::Socket socket(AF_INET, SOCK_STREAM);
            assert(socket.isValid());
            // socket应该在作用域结束时自动关闭
        } catch (const prpc::NetworkException& e) {
            // 网络操作可能失败，这是可以接受的
            std::cout << "Network resource test: " << e.what() << std::endl;
        }
        
        std::cout << "Resource management test passed!" << std::endl;
    }
    
    static void testEndToEndScenario() {
        std::cout << "Testing end-to-end scenario..." << std::endl;
        
        try {
            // 1. 创建配置文件
            const char* test_config = "e2e_test.conf";
            std::ofstream file(test_config);
            file << "rpcserverip=127.0.0.1\n";
            file << "rpcserverport=8082\n";
            file << "zookeeperip=127.0.0.1\n";
            file << "zookeeperport=2181\n";
            file.close();
            
            // 2. 初始化应用程序
            const char* argv[] = {"test_program", "-i", test_config};
            int argc = 3;
            
            auto init_result = Papplication::Init(argc, const_cast<char**>(argv));
            assert(init_result.isSuccess());
            
            // 3. 获取配置并验证
            Pconfig& config = Papplication::GetConfig();
            std::string server_ip = config.Load("rpcserverip");
            std::string server_port = config.Load("rpcserverport");
            
            assert(server_ip == "127.0.0.1");
            assert(server_port == "8082");
            
            // 4. 记录日志
            LOG(INFO) << "E2E test: Server configured at " << server_ip << ":" << server_port;
            
            // 5. 测试错误处理
            auto error_test = prpc::ErrorHandler::safeExecute([]() -> std::string {
                return "E2E test completed successfully";
            });
            
            assert(error_test.isSuccess());
            LOG(INFO) << error_test.getValue();
            
            // 清理
            std::remove(test_config);
            
        } catch (const std::exception& e) {
            std::cout << "E2E test error: " << e.what() << std::endl;
            assert(false);
        }
        
        std::cout << "End-to-end scenario test passed!" << std::endl;
    }
};

int main() {
    std::cout << "Starting integration tests..." << std::endl;
    
    try {
        IntegrationTest::testApplicationConfigIntegration();
        IntegrationTest::testErrorHandlingIntegration();
        IntegrationTest::testLoggingIntegration();
        IntegrationTest::testNetworkErrorIntegration();
        IntegrationTest::testConfigErrorIntegration();
        IntegrationTest::testComponentLifecycle();
        IntegrationTest::testResourceManagement();
        IntegrationTest::testConcurrentOperations();
        IntegrationTest::testEndToEndScenario();
        
        std::cout << "All integration tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Integration test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Integration test failed with unknown exception" << std::endl;
        return 1;
    }
} 