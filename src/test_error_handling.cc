#include "error.h"
#include "network_utils.h"
#include "logger.h"
#include "conf.h"
#include "application.h"
#include <iostream>
#include <cassert>

using namespace prpc;

// 测试函数
void testResultClass() {
    std::cout << "Testing Result class..." << std::endl;
    
    // 测试成功情况
    Result<int> success_result(42);
    assert(success_result.isSuccess());
    assert(success_result.getValue() == 42);
    assert(success_result.getErrorCode() == ErrorCode::SUCCESS);
    
    // 测试失败情况
    Result<int> error_result(ErrorCode::NETWORK_ERROR, "Connection failed");
    assert(!error_result.isSuccess());
    assert(error_result.getErrorCode() == ErrorCode::NETWORK_ERROR);
    assert(error_result.getErrorMessage() == "Connection failed");
    
    // 测试异常抛出
    try {
        error_result.getValue();
        assert(false); // 应该抛出异常
    } catch (const PrpcException& e) {
        assert(e.getErrorCode() == ErrorCode::NETWORK_ERROR);
    }
    
    std::cout << "Result class tests passed!" << std::endl;
}

void testExceptionClasses() {
    std::cout << "Testing exception classes..." << std::endl;
    
    try {
        throw ConfigException("Configuration error");
    } catch (const ConfigException& e) {
        assert(e.getErrorCode() == ErrorCode::CONFIG_ERROR);
        assert(std::string(e.what()).find("Configuration error") != std::string::npos);
    }
    
    try {
        throw NetworkException("Network error");
    } catch (const NetworkException& e) {
        assert(e.getErrorCode() == ErrorCode::NETWORK_ERROR);
    }
    
    try {
        throw ServiceException("Service error");
    } catch (const ServiceException& e) {
        assert(e.getErrorCode() == ErrorCode::SERVICE_ERROR);
    }
    
    std::cout << "Exception classes tests passed!" << std::endl;
}

void testErrorHandler() {
    std::cout << "Testing ErrorHandler..." << std::endl;
    
    // 测试safeExecute成功情况
    auto success_result = ErrorHandler::safeExecute([]() -> int {
        return 100;
    });
    assert(success_result.isSuccess());
    assert(success_result.getValue() == 100);
    
    // 测试safeExecute异常情况
    auto error_result = ErrorHandler::safeExecute([]() -> int {
        throw NetworkException("Test network error");
    });
    assert(!error_result.isSuccess());
    assert(error_result.getErrorCode() == ErrorCode::NETWORK_ERROR);
    
    // 测试全局错误处理器
    bool handler_called = false;
    ErrorHandler::setGlobalErrorHandler([&handler_called](const PrpcException& e) {
        handler_called = true;
        assert(e.getErrorCode() == ErrorCode::SERVICE_ERROR);
    });
    
    try {
        throw ServiceException("Test service error");
    } catch (const PrpcException& e) {
        ErrorHandler::handleException(e);
    }
    
    assert(handler_called);
    
    std::cout << "ErrorHandler tests passed!" << std::endl;
}

void testConfigErrorHandling() {
    std::cout << "Testing config error handling..." << std::endl;
    
    Pconfig config;
    
    // 测试空配置文件路径
    auto result1 = config.LoadConfigFile(nullptr);
    assert(!result1.isSuccess());
    assert(result1.getErrorCode() == ErrorCode::CONFIG_ERROR);
    
    // 测试不存在的配置文件
    auto result2 = config.LoadConfigFile("nonexistent_file.conf");
    assert(!result2.isSuccess());
    assert(result2.getErrorCode() == ErrorCode::CONFIG_ERROR);
    
    std::cout << "Config error handling tests passed!" << std::endl;
}

void testNetworkUtils() {
    std::cout << "Testing network utilities..." << std::endl;
    
    try {
        // 测试创建socket
        network::Socket socket(AF_INET, SOCK_STREAM);
        assert(socket.isValid());
        
        // 测试设置选项
        socket.setReuseAddr();
        socket.setKeepAlive();
        
        // 测试地址类
        network::Address addr("127.0.0.1", 8080);
        assert(addr.getIp() == "127.0.0.1");
        assert(addr.getPort() == 8080);
        
        std::cout << "Network utilities tests passed!" << std::endl;
        
    } catch (const PrpcException& e) {
        std::cout << "Network utilities test failed: " << e.what() << std::endl;
        assert(false);
    }
}

void testScopedResource() {
    std::cout << "Testing ScopedResource..." << std::endl;
    
    // 测试文件资源管理
    {
        FILE* fp = fopen("/dev/null", "r");
        ScopedResource<FILE*> file(fp);
        file.setCleanup([](FILE*& f) { if (f) { fclose(f); f = nullptr; } });
        
        if (file.get()) {
            assert(file.get() != nullptr);
        }
    } // 文件应该在这里自动关闭
    
    std::cout << "ScopedResource tests passed!" << std::endl;
}

int main() {
    std::cout << "Starting error handling tests..." << std::endl;
    
    try {
        testResultClass();
        testExceptionClasses();
        testErrorHandler();
        testConfigErrorHandling();
        testNetworkUtils();
        testScopedResource();
        
        std::cout << "All error handling tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
} 