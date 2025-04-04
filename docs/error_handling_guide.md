# PRPC 错误处理和异常安全指南

## 概述

PRPC框架现在提供了完整的错误处理和异常安全机制，包括：

- 统一的错误码系统
- 类型安全的异常类
- RAII资源管理
- 错误结果包装器
- 全局错误处理器

## 核心组件

### 1. 错误码枚举

```cpp
enum class ErrorCode {
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
```

### 2. 异常类层次结构

```cpp
PrpcException (基础异常类)
├── ConfigException
├── NetworkException
├── ZookeeperException
├── SerializationException
├── ServiceException
├── TimeoutException
└── 其他具体异常类
```

### 3. Result 模板类

用于包装函数返回值，提供类型安全的错误处理：

```cpp
template<typename T>
class Result {
    // 成功构造函数
    explicit Result(T value);
    
    // 错误构造函数
    explicit Result(ErrorCode code, std::string message = "");
    
    // 检查是否成功
    bool isSuccess() const;
    
    // 获取值（仅在成功时调用）
    const T& getValue() const;
    
    // 获取错误信息
    ErrorCode getErrorCode() const;
    const std::string& getErrorMessage() const;
};
```

## 使用示例

### 1. 基本错误处理

```cpp
#include "error.hpp"

// 使用Result返回类型
prpc::Result<int> safeDivide(int a, int b) {
    try {
        if (b == 0) {
            throw prpc::ServiceException("Division by zero");
        }
        return prpc::Result<int>(a / b);
    } catch (const prpc::PrpcException& e) {
        return prpc::Result<int>(e.getErrorCode(), e.what());
    }
}

// 使用函数
auto result = safeDivide(10, 2);
if (result.isSuccess()) {
    std::cout << "Result: " << result.getValue() << std::endl;
} else {
    std::cout << "Error: " << result.getErrorMessage() << std::endl;
}
```

### 2. RAII资源管理

```cpp
#include "error.hpp"

// 使用ScopedResource管理文件
prpc::Result<void> safeFileOperation(const std::string& filename) {
    try {
        FILE* fp = fopen(filename.c_str(), "r");
        prpc::ScopedResource<FILE*> file(fp, [](FILE* f) { 
            if (f) fclose(f); 
        });
        
        if (!file.get()) {
            throw prpc::ConfigException("Failed to open file: " + filename);
        }
        
        // 文件操作...
        return prpc::Result<void>();
    } catch (const prpc::PrpcException& e) {
        return prpc::Result<void>(e.getErrorCode(), e.what());
    }
}
```

### 3. 网络操作异常安全

```cpp
#include "network_utils.hpp"

prpc::Result<void> safeNetworkOperation(const std::string& ip, uint16_t port) {
    try {
        // 使用RAII Socket
        prpc::network::Socket socket = prpc::network::utils::createTcpClient(ip, port);
        socket.setTimeout(5000); // 5秒超时
        
        // 发送数据
        std::string message = "Hello, Server!";
        auto send_result = prpc::network::utils::safeSend(socket, message.data(), message.size());
        if (!send_result.isSuccess()) {
            return prpc::Result<void>(send_result.getErrorCode(), send_result.getErrorMessage());
        }
        
        return prpc::Result<void>();
    } catch (const prpc::PrpcException& e) {
        return prpc::Result<void>(e.getErrorCode(), e.what());
    }
}
```

### 4. 全局错误处理器

```cpp
#include "error.hpp"
#include "logger.hpp"

// 设置全局错误处理器
prpc::ErrorHandler::setGlobalErrorHandler([](const prpc::PrpcException& e) {
    LOG(ERROR) << "Global error handler caught: " << e.what() 
               << " (Error code: " << static_cast<int>(e.getErrorCode()) << ")";
    
    // 根据错误类型进行不同处理
    switch (e.getErrorCode()) {
        case prpc::ErrorCode::NETWORK_ERROR:
            LOG(ERROR) << "Network error occurred, attempting recovery...";
            break;
        case prpc::ErrorCode::TIMEOUT_ERROR:
            LOG(ERROR) << "Timeout error occurred, retrying...";
            break;
        case prpc::ErrorCode::CONFIG_ERROR:
            LOG(FATAL) << "Configuration error, cannot continue";
            break;
        default:
            LOG(ERROR) << "Unknown error type";
            break;
    }
});
```

### 5. 安全执行函数

```cpp
#include "error.hpp"

// 使用ErrorHandler::safeExecute包装可能抛出异常的函数
auto result = prpc::ErrorHandler::safeExecute([]() -> std::string {
    // 可能抛出异常的操作
    if (some_condition) {
        throw prpc::NetworkException("Network error");
    }
    return "Operation completed successfully";
});

if (result.isSuccess()) {
    std::cout << "Success: " << result.getValue() << std::endl;
} else {
    std::cout << "Error: " << result.getErrorMessage() << std::endl;
}
```

### 6. 链式错误处理

```cpp
prpc::Result<void> chainedOperations() {
    // 第一步：配置加载
    auto config_result = loadConfiguration();
    if (!config_result.isSuccess()) {
        LOG(ERROR) << "Config loading failed: " << config_result.getErrorMessage();
        return config_result;
    }
    
    // 第二步：网络连接
    auto network_result = establishConnection();
    if (!network_result.isSuccess()) {
        LOG(ERROR) << "Network connection failed: " << network_result.getErrorMessage();
        return network_result;
    }
    
    // 第三步：数据处理
    auto data_result = processData();
    if (!data_result.isSuccess()) {
        LOG(ERROR) << "Data processing failed: " << data_result.getErrorMessage();
        return prpc::Result<void>(data_result.getErrorCode(), data_result.getErrorMessage());
    }
    
    LOG(INFO) << "All operations completed successfully";
    return prpc::Result<void>();
}
```

## 最佳实践

### 1. 异常安全原则

- **基本保证**：确保程序状态一致
- **强保证**：操作要么成功，要么回滚到原始状态
- **不抛出保证**：确保函数不会抛出异常

### 2. 资源管理

- 使用RAII模式管理资源
- 优先使用智能指针
- 确保资源在异常情况下也能正确释放

### 3. 错误传播

- 使用Result类型返回错误信息
- 避免使用全局变量存储错误状态
- 在适当的地方记录错误日志

### 4. 异常处理策略

- 捕获具体的异常类型
- 避免捕获所有异常（catch(...)）除非必要
- 在析构函数中不要抛出异常

### 5. 性能考虑

- 异常处理有性能开销，不要用于控制流
- 在性能关键路径上使用错误码而不是异常
- 合理使用移动语义减少拷贝开销

## 迁移指南

### 从旧代码迁移

1. **替换exit()调用**：
   ```cpp
   // 旧代码
   if (error_condition) {
       exit(EXIT_FAILURE);
   }
   
   // 新代码
   if (error_condition) {
       throw prpc::ConfigException("Error description");
   }
   ```

2. **替换LOG(FATAL)**：
   ```cpp
   // 旧代码
   LOG(FATAL) << "Error message";
   
   // 新代码
   throw prpc::ServiceException("Error message");
   ```

3. **使用Result返回类型**：
   ```cpp
   // 旧代码
   bool loadConfig(const char* filename);
   
   // 新代码
   prpc::Result<void> loadConfig(const char* filename);
   ```

4. **使用RAII管理资源**：
   ```cpp
   // 旧代码
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   // ... 使用socket
   close(fd);
   
   // 新代码
   prpc::network::Socket socket(AF_INET, SOCK_STREAM);
   // ... 使用socket，自动关闭
   ```

## 测试

运行错误处理测试：

```bash
cd build
make test_error_handling
./test_error_handling
```

## 总结

新的错误处理机制提供了：

- **类型安全**：编译时错误检查
- **异常安全**：RAII资源管理
- **可维护性**：统一的错误处理模式
- **可扩展性**：易于添加新的错误类型
- **性能**：最小化异常处理开销

通过遵循这些指南，你可以构建更加健壮和可维护的PRPC应用程序。 