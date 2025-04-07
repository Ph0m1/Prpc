#ifndef TEST_DATA_GENERATOR_H
#define TEST_DATA_GENERATOR_H

#include <string>
#include <vector>
#include <random>
#include <fstream>

class TestDataGenerator {
public:
    static TestDataGenerator& getInstance() {
        static TestDataGenerator instance;
        return instance;
    }
    
    // 生成随机字符串
    std::string generateRandomString(size_t length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += chars[distribution_(generator_) % chars.size()];
        }
        return result;
    }
    
    // 生成随机整数
    int generateRandomInt(int min = 0, int max = 1000) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(generator_);
    }
    
    // 生成随机配置文件
    void generateConfigFile(const std::string& filename, size_t num_entries = 100) {
        std::ofstream file(filename);
        for (size_t i = 0; i < num_entries; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = generateRandomString(10);
            file << key << "=" << value << std::endl;
        }
    }
    
    // 生成测试网络数据
    std::vector<uint8_t> generateNetworkData(size_t size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(distribution_(generator_) % 256);
        }
        return data;
    }
    
    // 生成边界测试数据
    std::vector<std::string> generateBoundaryStrings() {
        return {
            "",                           // 空字符串
            "a",                         // 单字符
            generateRandomString(1024),   // 中等长度
            generateRandomString(65536),  // 大字符串
            std::string(1000, '\0'),     // 包含null字符
            "测试中文字符串",              // Unicode字符
            "Special!@#$%^&*()chars",    // 特殊字符
            "\n\r\t",                    // 控制字符
        };
    }
    
    // 生成压力测试数据
    std::vector<std::string> generateStressTestData(size_t count) {
        std::vector<std::string> data;
        data.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            size_t length = distribution_(generator_) % 1000 + 1;
            data.push_back(generateRandomString(length));
        }
        return data;
    }

private:
    TestDataGenerator() : generator_(std::random_device{}()), distribution_(0, RAND_MAX) {}
    
    std::mt19937 generator_;
    std::uniform_int_distribution<> distribution_;
};

#endif // TEST_DATA_GENERATOR_H 