#ifndef PRPC_ERROR_H
#define PRPC_ERROR_H

#include <stdexcept>
#include <string>
#include <system_error>
#include <memory>
#include <functional>
#include <cstring>
#include <cstdint>

namespace prpc {

// 错误码枚举
enum class ErrorCode : std::uint16_t {
    SUCCESS = 0,
    CONFIG_ERROR = 1000,
    NETWORK_ERROR = 2000,
    ZOOKEEPER_ERROR = 3000,
    SERIALIZATION_ERROR = 4000,
    SERVICE_ERROR = 5000,
    TIMEOUT_ERROR = 6000,
    INVALID_ARGUMENT = 7000,
    RESOURCE_ERROR = 8000,
    UNKNOWN_ERROR = 9999
};

// 错误码到字符串的转换
inline std::string ErrorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::CONFIG_ERROR: return "CONFIG_ERROR";
        case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
        case ErrorCode::ZOOKEEPER_ERROR: return "ZOOKEEPER_ERROR";
        case ErrorCode::SERIALIZATION_ERROR: return "SERIALIZATION_ERROR";
        case ErrorCode::SERVICE_ERROR: return "SERVICE_ERROR";
        case ErrorCode::TIMEOUT_ERROR: return "TIMEOUT_ERROR";
        case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ErrorCode::RESOURCE_ERROR: return "RESOURCE_ERROR";
        case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        default: return "UNKNOWN_ERROR";
    }
}

// 基础异常类
class PrpcException : public std::runtime_error {
public:
    explicit PrpcException(ErrorCode code, const std::string& message = "")
        : std::runtime_error(message.empty() ? ErrorCodeToString(code) : message)
        , error_code_(code) {}
    
    ErrorCode getErrorCode() const { return error_code_; }
    
private:
    ErrorCode error_code_;
};

// 具体异常类
class ConfigException : public PrpcException {
public:
    explicit ConfigException(const std::string& message = "")
        : PrpcException(ErrorCode::CONFIG_ERROR, message) {}
};

class NetworkException : public PrpcException {
public:
    explicit NetworkException(const std::string& message = "")
        : PrpcException(ErrorCode::NETWORK_ERROR, message) {}
};

class ZookeeperException : public PrpcException {
public:
    explicit ZookeeperException(const std::string& message = "")
        : PrpcException(ErrorCode::ZOOKEEPER_ERROR, message) {}
};

class SerializationException : public PrpcException {
public:
    explicit SerializationException(const std::string& message = "")
        : PrpcException(ErrorCode::SERIALIZATION_ERROR, message) {}
};

class ServiceException : public PrpcException {
public:
    explicit ServiceException(const std::string& message = "")
        : PrpcException(ErrorCode::SERVICE_ERROR, message) {}
};

class TimeoutException : public PrpcException {
public:
    explicit TimeoutException(const std::string& message = "")
        : PrpcException(ErrorCode::TIMEOUT_ERROR, message) {}
};

// 错误结果类 - 用于返回错误信息
template<typename T>
class Result {
public:
    // 成功构造函数
    explicit Result(T value) : value_(std::move(value)), error_code_(ErrorCode::SUCCESS) {}
    
    // 错误构造函数
    explicit Result(ErrorCode code, std::string message = "")
        : value_{}, error_code_(code), error_message_(std::move(message)) {}
    
    // 检查是否成功
    bool isSuccess() const { return error_code_ == ErrorCode::SUCCESS; }
    
    // 获取值（仅在成功时调用）
    const T& getValue() const {
        if (!isSuccess()) {
            throw PrpcException(error_code_, error_message_);
        }
        return value_;
    }
    
    // 获取错误码
    ErrorCode getErrorCode() const { return error_code_; }
    
    // 获取错误信息
    const std::string& getErrorMessage() const { return error_message_; }
    
    // 转换为值（如果失败则抛出异常）
    T operator*() const { return getValue(); }
    
private:
    T value_;
    ErrorCode error_code_;
    std::string error_message_;
};

// 特化版本用于void返回类型
template<>
class Result<void> {
public:
    // 成功构造函数
    Result() : error_code_(ErrorCode::SUCCESS) {}
    
    // 错误构造函数
    explicit Result(ErrorCode code, std::string message = "")
        : error_code_(code), error_message_(std::move(message)) {}
    
    bool isSuccess() const { return error_code_ == ErrorCode::SUCCESS; }
    ErrorCode getErrorCode() const { return error_code_; }
    const std::string& getErrorMessage() const { return error_message_; }
    
private:
    ErrorCode error_code_;
    std::string error_message_;
};

// 错误处理工具类
class ErrorHandler {
public:
    // 设置全局错误处理器
    static void setGlobalErrorHandler(std::function<void(const PrpcException&)> handler) {
        global_error_handler_ = std::move(handler);
    }
    
    // 处理异常
    static void handleException(const PrpcException& e) {
        if (global_error_handler_) {
            global_error_handler_(e);
        } else {
            // 默认处理：记录日志并重新抛出
            throw;
        }
    }
    
    // 安全执行函数
    template<typename Func>
    static auto safeExecute(Func&& func) -> Result<decltype(func())> {
        try {
            return Result<decltype(func())>(func());
        } catch (const PrpcException& e) {
            // 记录错误但不重新抛出，确保函数安全
            if (global_error_handler_) {
                try {
                    global_error_handler_(e);
                } catch (...) {
                    // 忽略全局处理器的异常，保证safeExecute的安全性
                }
            }
            return Result<decltype(func())>(e.getErrorCode(), e.what());
        } catch (const std::exception& e) {
            return Result<decltype(func())>(ErrorCode::UNKNOWN_ERROR, e.what());
        } catch (...) {
            return Result<decltype(func())>(ErrorCode::UNKNOWN_ERROR, "Unknown error occurred");
        }
    }
    
private:
    static std::function<void(const PrpcException&)> global_error_handler_;
};

// RAII资源管理基类
template<typename Resource>
class ScopedResource {
public:
    template<typename... Args>
    explicit ScopedResource(Args&&... args) 
        : resource_(std::forward<Args>(args)...) {}
    
    ~ScopedResource() {
        if (cleanup_) {
            cleanup_(resource_);
        }
    }
    
    // 禁用拷贝
    ScopedResource(const ScopedResource&) = delete;
    ScopedResource& operator=(const ScopedResource&) = delete;
    
    // 允许移动
    ScopedResource(ScopedResource&&) = default;
    ScopedResource& operator=(ScopedResource&&) = default;
    
    // 访问资源
    Resource& get() { return resource_; }
    const Resource& get() const { return resource_; }
    
    // 设置清理函数
    void setCleanup(std::function<void(Resource&)> cleanup) {
        cleanup_ = std::move(cleanup);
    }
    
    // 释放资源（转移所有权）
    Resource release() {
        cleanup_ = nullptr;
        return std::move(resource_);
    }
    
private:
    Resource resource_;
    std::function<void(Resource&)> cleanup_;
};

// 网络相关错误处理
namespace network {
    // 检查系统调用结果
    inline void checkSystemCall(int result, const std::string& operation) {
        if (result == -1) {
            throw NetworkException(operation + " failed: " + std::strerror(errno));
        }
    }
    
    // 检查socket操作
    inline void checkSocketOperation(int result, const std::string& operation) {
        if (result == -1) {
            throw NetworkException(operation + " failed: " + std::strerror(errno));
        }
    }
}

// 配置相关错误处理
namespace config {
    inline void checkConfigFile(const std::string& filename) {
        if (filename.empty()) {
            throw ConfigException("Configuration file path is empty");
        }
    }
}

} // namespace prpc

#endif // PRPC_ERROR_H 