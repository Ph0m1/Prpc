#include <stdint.h>
#include <stddef.h>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include "conf.h"
#include "logger.h"
#include "error.h"

using namespace prpc;

// 配置解析模糊测试
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;
    
    try {
        // 将输入数据转换为字符串
        std::string input(reinterpret_cast<const char*>(data), size);
        
        // 创建临时配置文件
        std::string temp_file = "/tmp/fuzz_config_" + std::to_string(rand()) + ".conf";
        std::ofstream file(temp_file);
        file << input;
        file.close();
        
        // 测试配置解析
        Pconfig config;
        try {
            config.LoadConfigFile(temp_file.c_str());
            
            // 尝试读取一些键值
            config.Load("test_key");
            config.Load("");
            config.Load("very_long_key_name_that_might_cause_issues");
            
        } catch (const std::exception& e) {
            // 预期的异常，忽略
        }
        
        // 清理临时文件
        std::remove(temp_file.c_str());
        
    } catch (...) {
        // 捕获所有异常，防止崩溃
    }
    
    return 0;
}

// 日志模糊测试
extern "C" int LLVMFuzzerTestOneInputLogger(const uint8_t *data, size_t size) {
    if (size == 0) return 0;
    
    try {
        std::string input(reinterpret_cast<const char*>(data), size);
        
        // 测试日志输出
        PLogger& logger = PLogger::getInstance();
        
        // 尝试记录各种格式的日志
        LOG(INFO) << input;
        LOG(ERROR) << "Error: " << input;
        
    } catch (...) {
        // 捕获所有异常
    }
    
    return 0;
}

// 错误处理模糊测试
extern "C" int LLVMFuzzerTestOneInputError(const uint8_t *data, size_t size) {
    if (size == 0) return 0;
    
    try {
        std::string input(reinterpret_cast<const char*>(data), size);
        
        // 测试错误消息处理
        try {
            throw PrpcException(ErrorCode::UNKNOWN_ERROR, input);
        } catch (const PrpcException& e) {
            // 测试错误消息访问
            [[maybe_unused]] std::string msg = e.what();
            [[maybe_unused]] ErrorCode code = e.getErrorCode();
        }
        
        // 测试Result类
        auto result = Result<std::string>(ErrorCode::NETWORK_ERROR, input);
        if (!result.isSuccess()) {
            [[maybe_unused]] std::string error_msg = result.getErrorMessage();
        }
        
    } catch (...) {
        // 捕获所有异常
    }
    
    return 0;
} 