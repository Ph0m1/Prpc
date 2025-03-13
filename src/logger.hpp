#ifndef PRPC_LOGGER_HPP
#define PRPC_LOGGER_HPP

#include <iostream>
#include <string>
#include <sstream> // 用于实现流式接口
#include <chrono>
#include <ctime>
#include <mutex>
#include <thread>

// 定义日志级别
enum LogLevel {
    INFO,
    ERROR,
    FATAL
};

class LogStream; // 前向声明

// 日志后端：线程安全的单例 PLogger
// 它负责将最终格式化的字符串写入到控制台
class PLogger {
public:
    // 获取日志类的唯一实例
    static PLogger& getInstance() {
        static PLogger instance;
        return instance;
    }

    // 禁止拷贝和赋值
    PLogger(const PLogger&) = delete;
    PLogger& operator=(const PLogger&) = delete;

    // 设置要记录的最低日志级别
    void setLogLevel(LogLevel level) {
        logLevel_ = level;
    }

    // 由 LogStream 的析构函数调用，负责写入最终的日志消息
    void log(LogLevel level, const char* file, int line, const std::string& message) {
        if (level < logLevel_) {
            return;
        }

        // 加锁以保证多线程输出的原子性
        std::lock_guard<std::mutex> lock(mtx_);

        // 1. 获取当前时间并格式化
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        char time_buf[32];
        std::tm tm_buf;
        localtime_r(&now_c, &tm_buf); // 使用线程安全的 localtime_r
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_buf);

        // 2. 选择输出流 (INFO -> cout, ERROR/FATAL -> cerr)
        std::ostream& stream = (level == INFO) ? std::cout : std::cerr;

        // 3. 构造日志前缀并输出
        stream << "[" << getLevelString(level) << "]";
        stream << "[" << time_buf << "." << ms.count() << "]";
        stream << "[" << file << ":" << line << "] ";
        stream << message << std::endl;
    }

private:
    // 构造函数私有化
    PLogger() : logLevel_(INFO) {}
    ~PLogger() = default;

    const char* getLevelString(LogLevel level) {
        switch(level) {
            case INFO:  return "INFO";
            case ERROR: return "ERROR";
            case FATAL: return "FATAL";
            default:    return "UNKNOWN";
        }
    }

    LogLevel logLevel_;
    std::mutex mtx_;
};


// 日志前端：LogStream 类
// LOG(INFO) 宏会创建一个此类的临时对象。
// 当这个临时对象被销毁时（在语句末尾），它的析构函数会被调用。
class LogStream {
public:
    LogStream(LogLevel level, const char* file, int line)
        : level_(level), file_(file), line_(line) {}

    // 析构函数：将缓冲区中的所有数据提交给后端的 PLogger
    ~LogStream() {
        PLogger::getInstance().log(level_, file_, line_, buffer_.str());
        // 如果是 FATAL 级别，则在打印日志后终止程序
        if (level_ == FATAL) {
            exit(EXIT_FAILURE);
        }
    }

    // 重载 operator<< 以便可以接收各种类型的数据
    template <typename T>
    LogStream& operator<<(const T& value) {
        buffer_ << value;
        return *this;
    }

private:
    LogLevel level_;
    const char* file_;
    int line_;
    std::stringstream buffer_; // 用于缓存流式输入的数据
};

// 定义日志宏，这是用户使用的唯一接口
#define LOG(level) LogStream(level, __FILE__, __LINE__)

#endif // PRPC_LOGGER_HPP
