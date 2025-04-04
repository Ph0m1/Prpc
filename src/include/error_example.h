#ifndef PRPC_ERROR_EXAMPLE_H
#define PRPC_ERROR_EXAMPLE_H

#include "error.h"
#include "network_utils.h"
#include "logger.h"
#include <iostream>

namespace prpc {
namespace example {

// 示例：如何使用新的错误处理机制
class ErrorHandlingExample {
public:
    // 示例1：使用Result返回类型
    static Result<int> safeDivide(int a, int b) {
        try {
            if (b == 0) {
                throw ServiceException("Division by zero");
            }
            return Result<int>(a / b);
        } catch (const PrpcException& e) {
            return Result<int>(e.getErrorCode(), e.what());
        }
    }
    
    // 示例2：使用RAII资源管理
    static Result<void> safeFileOperation(const std::string& filename) {
        try {
            // 使用RAII管理文件资源
            ScopedResource<FILE*> file(
                fopen(filename.c_str(), "r"),
                [](FILE* f) { if (f) fclose(f); }
            );
            
            if (!file.get()) {
                throw ConfigException("Failed to open file: " + filename);
            }
            
            // 文件操作...
            return Result<void>();
        } catch (const PrpcException& e) {
            return Result<void>(e.getErrorCode(), e.what());
        }
    }
    
    // 示例3：网络操作异常安全
    static Result<void> safeNetworkOperation(const std::string& ip, uint16_t port) {
        try {
            // 使用RAII Socket
            network::Socket socket = network::utils::createTcpClient(ip, port);
            socket.setTimeout(5000); // 5秒超时
            
            // 发送数据
            std::string message = "Hello, Server!";
            auto send_result = network::utils::safeSend(socket, message.data(), message.size());
            if (!send_result.isSuccess()) {
                LOG(ERROR) << "Failed to send data: " << send_result.getErrorMessage();
                return Result<void>(send_result.getErrorCode(), send_result.getErrorMessage());
            }
            
            // 接收数据
            char buffer[1024];
            auto recv_result = network::utils::safeRecv(socket, buffer, sizeof(buffer));
            if (!recv_result.isSuccess()) {
                LOG(ERROR) << "Failed to receive data: " << recv_result.getErrorMessage();
                return Result<void>(recv_result.getErrorCode(), recv_result.getErrorMessage());
            }
            
            return Result<void>();
        } catch (const PrpcException& e) {
            return Result<void>(e.getErrorCode(), e.what());
        }
    }
    
    // 示例4：设置全局错误处理器
    static void setupGlobalErrorHandler() {
        ErrorHandler::setGlobalErrorHandler([](const PrpcException& e) {
            LOG(ERROR) << "Global error handler caught: " << e.what() 
                       << " (Error code: " << static_cast<int>(e.getErrorCode()) << ")";
            
            // 可以根据错误类型进行不同的处理
            switch (e.getErrorCode()) {
                case ErrorCode::NETWORK_ERROR:
                    LOG(ERROR) << "Network error occurred, attempting recovery...";
                    break;
                case ErrorCode::TIMEOUT_ERROR:
                    LOG(ERROR) << "Timeout error occurred, retrying...";
                    break;
                case ErrorCode::CONFIG_ERROR:
                    LOG(FATAL) << "Configuration error, cannot continue";
                    break;
                default:
                    LOG(ERROR) << "Unknown error type";
                    break;
            }
        });
    }
    
    // 示例5：使用ErrorHandler::safeExecute
    static Result<std::string> safeExecuteExample() {
        return ErrorHandler::safeExecute([]() -> std::string {
            // 模拟可能抛出异常的操作
            if (rand() % 2 == 0) {
                throw NetworkException("Random network error");
            }
            return "Operation completed successfully";
        });
    }
    
    // 示例6：链式错误处理
    static Result<void> chainedErrorHandling() {
        // 第一步：配置加载
        auto config_result = safeFileOperation("config.txt");
        if (!config_result.isSuccess()) {
            LOG(ERROR) << "Config loading failed: " << config_result.getErrorMessage();
            return config_result;
        }
        
        // 第二步：网络连接
        auto network_result = safeNetworkOperation("127.0.0.1", 8080);
        if (!network_result.isSuccess()) {
            LOG(ERROR) << "Network operation failed: " << network_result.getErrorMessage();
            return network_result;
        }
        
        // 第三步：数据处理
        auto data_result = safeExecuteExample();
        if (!data_result.isSuccess()) {
            LOG(ERROR) << "Data processing failed: " << data_result.getErrorMessage();
            return Result<void>(data_result.getErrorCode(), data_result.getErrorMessage());
        }
        
        LOG(INFO) << "All operations completed successfully: " << data_result.getValue();
        return Result<void>();
    }
};

// 示例：如何在实际代码中使用
class RpcServiceExample {
public:
    // 安全的RPC调用
    static Result<void> safeRpcCall(const std::string& service_name, 
                                   const std::string& method_name,
                                   const std::string& request_data) {
        try {
            // 1. 参数验证
            if (service_name.empty() || method_name.empty()) {
                throw PrpcException(ErrorCode::INVALID_ARGUMENT, "Service name or method name is empty");
            }
            
            // 2. 网络连接
            network::Socket socket = network::utils::createTcpClient("127.0.0.1", 8080);
            socket.setTimeout(10000); // 10秒超时
            
            // 3. 序列化请求
            std::string serialized_request;
            if (!serializeRequest(service_name, method_name, request_data, serialized_request)) {
                throw SerializationException("Failed to serialize request");
            }
            
            // 4. 发送请求
            auto send_result = network::utils::safeSend(socket, serialized_request.data(), 
                                                      serialized_request.size());
            if (!send_result.isSuccess()) {
                return Result<void>(send_result.getErrorCode(), send_result.getErrorMessage());
            }
            
            // 5. 接收响应
            char response_buffer[4096];
            auto recv_result = network::utils::safeRecv(socket, response_buffer, sizeof(response_buffer));
            if (!recv_result.isSuccess()) {
                return Result<void>(recv_result.getErrorCode(), recv_result.getErrorMessage());
            }
            
            // 6. 处理响应
            if (!processResponse(response_buffer, recv_result.getValue())) {
                throw ServiceException("Failed to process response");
            }
            
            return Result<void>();
            
        } catch (const PrpcException& e) {
            return Result<void>(e.getErrorCode(), e.what());
        } catch (const std::exception& e) {
            return Result<void>(ErrorCode::UNKNOWN_ERROR, e.what());
        }
    }
    
private:
    static bool serializeRequest(const std::string& service_name,
                               const std::string& method_name,
                               const std::string& request_data,
                               std::string& serialized) {
        // 模拟序列化
        serialized = service_name + ":" + method_name + ":" + request_data;
        return true;
    }
    
    static bool processResponse(const char* response_data, ssize_t response_size) {
        // 模拟响应处理
        return response_size > 0;
    }
};

} // namespace example
} // namespace prpc

#endif // PRPC_ERROR_EXAMPLE_H 