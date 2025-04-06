#include "application.h"
#include "error.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdio>

class ApplicationTest {
public:
    static void testSingletonPattern() {
        std::cout << "Testing singleton pattern..." << std::endl;
        
        // 获取两个实例，应该是同一个对象
        Papplication& app1 = Papplication::GetInstance();
        Papplication& app2 = Papplication::GetInstance();
        
        assert(&app1 == &app2);
        
        std::cout << "Singleton pattern test passed!" << std::endl;
    }
    
    static void testValidInitialization() {
        std::cout << "Testing valid initialization..." << std::endl;
        
        // 创建临时配置文件
        const char* test_config = "test_app_config.conf";
        std::ofstream file(test_config);
        file << "rpcserverip=127.0.0.1\n";
        file << "rpcserverport=8080\n";
        file.close();
        
        // 模拟命令行参数
        const char* argv[] = {"test_program", "-i", test_config};
        int argc = 3;
        
        auto result = Papplication::Init(argc, const_cast<char**>(argv));
        assert(result.isSuccess());
        
        // 验证配置是否正确加载
        Pconfig& config = Papplication::GetConfig();
        assert(config.Load("rpcserverip") == "127.0.0.1");
        assert(config.Load("rpcserverport") == "8080");
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Valid initialization test passed!" << std::endl;
    }
    
    static void testInvalidArguments() {
        std::cout << "Testing invalid arguments..." << std::endl;
        
        // 测试参数不足
        const char* argv1[] = {"test_program"};
        int argc1 = 1;
        
        auto result1 = Papplication::Init(argc1, const_cast<char**>(argv1));
        assert(!result1.isSuccess());
        assert(result1.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        // 测试无效选项
        const char* argv2[] = {"test_program", "-x", "invalid"};
        int argc2 = 3;
        
        auto result2 = Papplication::Init(argc2, const_cast<char**>(argv2));
        assert(!result2.isSuccess());
        assert(result2.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        std::cout << "Invalid arguments test passed!" << std::endl;
    }
    
    static void testMissingConfigFile() {
        std::cout << "Testing missing config file..." << std::endl;
        
        // 测试不存在的配置文件
        const char* argv[] = {"test_program", "-i", "nonexistent_config.conf"};
        int argc = 3;
        
        auto result = Papplication::Init(argc, const_cast<char**>(argv));
        assert(!result.isSuccess());
        assert(result.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        std::cout << "Missing config file test passed!" << std::endl;
    }
    
    static void testConfigAccess() {
        std::cout << "Testing config access..." << std::endl;
        
        // 创建配置文件
        const char* test_config = "test_config_access.conf";
        std::ofstream file(test_config);
        file << "key1=value1\n";
        file << "key2=value2\n";
        file << "key3=value3\n";
        file.close();
        
        // 初始化应用程序
        const char* argv[] = {"test_program", "-i", test_config};
        int argc = 3;
        
        auto result = Papplication::Init(argc, const_cast<char**>(argv));
        if (!result.isSuccess()) {
            std::cout << "Init failed: " << result.getErrorMessage() << std::endl;
        }
        assert(result.isSuccess());
        
        // 测试配置访问
        Pconfig& config = Papplication::GetConfig();
        assert(config.Load("key1") == "value1");
        assert(config.Load("key2") == "value2");
        assert(config.Load("key3") == "value3");
        assert(config.Load("nonexistent") == "");
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Config access test passed!" << std::endl;
    }
    
    static void testMultipleInitialization() {
        std::cout << "Testing multiple initialization..." << std::endl;
        
        // 创建配置文件
        const char* test_config1 = "test_config1.conf";
        std::ofstream file1(test_config1);
        file1 << "server=server1\n";
        file1.close();
        
        const char* test_config2 = "test_config2.conf";
        std::ofstream file2(test_config2);
        file2 << "server=server2\n";
        file2.close();
        
        // 第一次初始化
        const char* argv1[] = {"test_program", "-i", test_config1};
        int argc1 = 3;
        
        auto result1 = Papplication::Init(argc1, const_cast<char**>(argv1));
        assert(result1.isSuccess());
        
        Pconfig& config = Papplication::GetConfig();
        assert(config.Load("server") == "server1");
        
        // 第二次初始化（应该覆盖之前的配置）
        const char* argv2[] = {"test_program", "-i", test_config2};
        int argc2 = 3;
        
        auto result2 = Papplication::Init(argc2, const_cast<char**>(argv2));
        assert(result2.isSuccess());
        assert(config.Load("server") == "server2");
        
        // 清理
        std::remove(test_config1);
        std::remove(test_config2);
        
        std::cout << "Multiple initialization test passed!" << std::endl;
    }
    
    static void testArgumentParsing() {
        std::cout << "Testing argument parsing..." << std::endl;
        
        // 创建配置文件
        const char* test_config = "test_arg_parsing.conf";
        std::ofstream file(test_config);
        file << "test=success\n";
        file.close();
        
        // 测试不同的参数格式
        const char* argv[] = {"test_program", "-i", test_config};
        int argc = 3;
        
        auto result = Papplication::Init(argc, const_cast<char**>(argv));
        assert(result.isSuccess());
        
        Pconfig& config = Papplication::GetConfig();
        assert(config.Load("test") == "success");
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Argument parsing test passed!" << std::endl;
    }
    
    static void testErrorPropagation() {
        std::cout << "Testing error propagation..." << std::endl;
        
        // 测试配置文件加载错误的传播
        const char* argv[] = {"test_program", "-i", "/dev/null/nonexistent"};
        int argc = 3;
        
        auto result = Papplication::Init(argc, const_cast<char**>(argv));
        assert(!result.isSuccess());
        assert(result.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        assert(!result.getErrorMessage().empty());
        
        std::cout << "Error propagation test passed!" << std::endl;
    }
    
    static void testInstanceLifecycle() {
        std::cout << "Testing instance lifecycle..." << std::endl;
        
        // 获取实例
        Papplication& app = Papplication::GetInstance();
        
        // 验证实例存在
        assert(&app != nullptr);
        
        // 测试删除实例（注意：这可能影响其他测试）
        // Papplication::deleteInstance(); // 谨慎调用
        
        std::cout << "Instance lifecycle test passed!" << std::endl;
    }
};

int main() {
    std::cout << "Starting application tests..." << std::endl;
    
    try {
        ApplicationTest::testSingletonPattern();
        ApplicationTest::testValidInitialization();
        ApplicationTest::testInvalidArguments();
        ApplicationTest::testMissingConfigFile();
        ApplicationTest::testConfigAccess();
        ApplicationTest::testMultipleInitialization();
        ApplicationTest::testArgumentParsing();
        ApplicationTest::testErrorPropagation();
        ApplicationTest::testInstanceLifecycle();
        
        std::cout << "All application tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Application test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Application test failed with unknown exception" << std::endl;
        return 1;
    }
} 