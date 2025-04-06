#include "conf.h"
#include "error.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdio>

class ConfigTest {
public:
    static void testLoadValidConfig() {
        std::cout << "Testing valid config loading..." << std::endl;
        
        // 创建临时配置文件
        const char* test_config = "test_config.conf";
        std::ofstream file(test_config);
        file << "# Test configuration\n";
        file << "rpcserverip=127.0.0.1\n";
        file << "rpcserverport=8080\n";
        file << "zookeeperip=127.0.0.1\n";
        file << "zookeeperport=2181\n";
        file << "\n";
        file << "# Comment line\n";
        file << "timeout=5000\n";
        file.close();
        
        Pconfig config;
        auto result = config.LoadConfigFile(test_config);
        
        assert(result.isSuccess());
        assert(config.Load("rpcserverip") == "127.0.0.1");
        assert(config.Load("rpcserverport") == "8080");
        assert(config.Load("zookeeperip") == "127.0.0.1");
        assert(config.Load("zookeeperport") == "2181");
        assert(config.Load("timeout") == "5000");
        assert(config.Load("nonexistent") == "");
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Valid config loading test passed!" << std::endl;
    }
    
    static void testLoadInvalidConfig() {
        std::cout << "Testing invalid config loading..." << std::endl;
        
        Pconfig config;
        
        // 测试空指针
        auto result1 = config.LoadConfigFile(nullptr);
        assert(!result1.isSuccess());
        assert(result1.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        // 测试不存在的文件
        auto result2 = config.LoadConfigFile("nonexistent_file.conf");
        assert(!result2.isSuccess());
        assert(result2.getErrorCode() == prpc::ErrorCode::CONFIG_ERROR);
        
        std::cout << "Invalid config loading test passed!" << std::endl;
    }
    
    static void testConfigWithSpecialCharacters() {
        std::cout << "Testing config with special characters..." << std::endl;
        
        const char* test_config = "test_special_config.conf";
        std::ofstream file(test_config);
        file << "key_with_spaces = value with spaces \n";
        file << "  key_with_leading_spaces=value\n";
        file << "key_with_trailing_spaces=value  \n";
        file << "empty_value=\n";
        file << "key_with_equals=value=with=equals\n";
        file.close();
        
        Pconfig config;
        auto result = config.LoadConfigFile(test_config);
        
        assert(result.isSuccess());
        assert(config.Load("key_with_spaces") == "value with spaces");
        assert(config.Load("key_with_leading_spaces") == "value");
        assert(config.Load("key_with_trailing_spaces") == "value");
        assert(config.Load("empty_value") == "");
        assert(config.Load("key_with_equals") == "value=with=equals");
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Special characters config test passed!" << std::endl;
    }
    
    static void testConfigWithComments() {
        std::cout << "Testing config with comments..." << std::endl;
        
        const char* test_config = "test_comments_config.conf";
        std::ofstream file(test_config);
        file << "# This is a comment\n";
        file << "valid_key=valid_value\n";
        file << "# Another comment\n";
        file << "\n";  // Empty line
        file << "another_key=another_value\n";
        file << "#commented_key=commented_value\n";
        file.close();
        
        Pconfig config;
        auto result = config.LoadConfigFile(test_config);
        
        assert(result.isSuccess());
        assert(config.Load("valid_key") == "valid_value");
        assert(config.Load("another_key") == "another_value");
        assert(config.Load("commented_key") == "");  // Should not exist
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Comments config test passed!" << std::endl;
    }
    
    static void testConfigReload() {
        std::cout << "Testing config reload..." << std::endl;
        
        const char* test_config = "test_reload_config.conf";
        
        // 创建第一个配置
        std::ofstream file1(test_config);
        file1 << "key1=value1\n";
        file1 << "key2=value2\n";
        file1.close();
        
        Pconfig config;
        auto result1 = config.LoadConfigFile(test_config);
        assert(result1.isSuccess());
        assert(config.Load("key1") == "value1");
        assert(config.Load("key2") == "value2");
        
        // 修改配置文件
        std::ofstream file2(test_config);
        file2 << "key1=new_value1\n";
        file2 << "key3=value3\n";
        file2.close();
        
        // 重新加载
        auto result2 = config.LoadConfigFile(test_config);
        assert(result2.isSuccess());
        assert(config.Load("key1") == "new_value1");
        assert(config.Load("key2") == "");  // Should be cleared
        assert(config.Load("key3") == "value3");
        
        // 清理
        std::remove(test_config);
        
        std::cout << "Config reload test passed!" << std::endl;
    }
};

int main() {
    std::cout << "Starting configuration tests..." << std::endl;
    
    try {
        ConfigTest::testLoadValidConfig();
        ConfigTest::testLoadInvalidConfig();
        ConfigTest::testConfigWithSpecialCharacters();
        ConfigTest::testConfigWithComments();
        ConfigTest::testConfigReload();
        
        std::cout << "All configuration tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Configuration test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Configuration test failed with unknown exception" << std::endl;
        return 1;
    }
} 