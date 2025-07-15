## Prpc - 轻量高性能 C++17 分布式 RPC 框架

Prpc 是在 Linux (x86_64) 环境下，使用 C++17 开发的分布式 RPC 框架。项目采用主从 Reactor 架构，结合非阻塞 I/O 与 Epoll 以及线程池实现高并发；基于 Protobuf 构建自定义协议并完成请求路由与消息编解码；利用 ZooKeeper 实现服务注册与发现；提供完善的日志、错误处理与对象池优化，并配套单元/集成/性能/模糊测试。

---

### 特性概览
- **架构**: 主从 Reactor + 非阻塞 I/O + Epoll + 线程池，提升并发吞吐。
- **协议**: 使用 Protobuf 定义与序列化消息，支持请求路由与解析。
- **服务治理**: 集成 ZooKeeper，提供服务注册、发现、动态获取服务地址与解耦。
- **连接管理**: 客户端连接池复用 TCP 连接，降低建连开销；超时与重连机制保障稳定性。
- **日志系统**: 支持按级别过滤，包含时间戳、代码位置，便于调试与追踪。
- **错误处理**: 自定义 ErrorCode、PrpcException、Result<T>、ErrorHandler，异常安全与可观测。
- **资源管理**: RAII 封装（如 Socket、ScopedResource）与对象池优化（消息池/缓冲池）。
- **测试与质量**: 覆盖单元/集成/性能/模糊测试，支持 clang-tidy、cppcheck 与覆盖率统计。

---

### 目录结构
```
prpc/
  ├─ src/                 # 框架核心源码
  │  ├─ include/          # 公开头文件（.h）
  │  ├─ *.cc              # 具体实现
  │  ├─ CMakeLists.txt
  ├─ sample/              # 示例：caller/callee
  ├─ tests/               # 单元/集成/性能/模糊测试与脚本
  ├─ docs/                # 文档（错误处理、优化指南等）
  ├─ bin/                 # 运行脚本/二进制示例
  ├─ CMakeLists.txt       # 项目根 CMake 配置
  ├─ .clang-tidy          # clang-tidy 配置
  └─ .gitignore
```

---

### 环境依赖
- Linux（x86_64）
- C++17 编译器（GCC 9+/Clang 10+）
- CMake 3.15+
- Protobuf（protoc 与 libprotobuf）
- ZooKeeper 客户端库（libzookeeper-mt 等）
- 可选：cppcheck、clang-tidy、lcov（静态分析与覆盖率）

---

### 构建与安装
```
# 1. 生成构建目录
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 2. 构建
cmake --build build -j

# 3. 可选：安装（若 CMake 配置支持）
cmake --install build
```

如需 Debug 构建：
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

---

### 快速开始
- 运行示例服务端与客户端（参考 `sample/` 与 `bin/` 内脚本或可执行文件）
```
# 生成示例并运行（构建后）
./build/sample/callee/server
./build/sample/caller/client
```
- 配置文件示例见 `bin/test.conf`，可通过 `src/application.cc` 中的参数解析指定配置路径。

---

### 测试与性能
项目提供断言风格的测试与脚本：
```
# 运行测试脚本（构建 + 执行 + 生成报告）
./tests/run_tests.sh
```
- 单元测试：配置、日志、线程池、网络工具、错误处理、对象池等。
- 集成测试：应用-配置、错误处理-日志-网络等链路。
- 性能测试：日志吞吐、线程池、对象池与内存分配对比等。
- 模糊测试：基于 libFuzzer 的输入扰动测试（如开启）。

测试报告脚本：`tests/generate_test_report.py` 会输出 HTML/JSON。

---

### 常见问题（FAQ）
- 编译失败：确认已安装 Protobuf 与 ZooKeeper 开发库；检查 `CMAKE_PREFIX_PATH` 与库路径。
- 端口/网络问题：确保端口在 `uint16_t` 范围内（如 9999），并具备权限与防火墙放行。
- getopt 重入：多次调用初始化需重置 `optind = 1`（已在 `Papplication::Init` 中处理）。
- clang-tidy 告警：已在 `.clang-tidy` 配置中适配，可按需调整本地规则。

---

### 设计要点
- 错误处理：`ErrorCode` 使用 `std::uint16_t`，`Result<T>` 提供 `has_value()/error()`，并含 `void` 特化；`ErrorHandler` 提供 `safeExecute` 与全局处理器。
- 资源与网络：`prpc::network::Socket` RAII 封装、`setTimeout` 类型安全处理；提供 `createTcpServer/Client`、`safeSend/Recv`。
- 对象池：`ObjectPool` 泛型池与 `MessagePool`（消息/缓冲池），含统计监控 `PoolMonitor`。
- 并发：线程池 `submit` 接口，使用 `std::invoke_result` 规避弃用项。

--- 