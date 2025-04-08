#ifndef MESSAGE_POOL_H
#define MESSAGE_POOL_H

#include "object_pool.h"
#include <string>
#include <vector>
#include <cstring>

namespace prpc {

/**
 * @brief RPC消息结构
 */
struct RpcMessage {
    std::string method_name;
    std::vector<uint8_t> payload;
    uint64_t request_id;
    uint32_t timeout_ms;
    bool is_response;
    int32_t error_code;
    std::string error_message;
    
    // 预分配缓冲区大小
    static constexpr size_t DEFAULT_PAYLOAD_SIZE = 4096;
    
    RpcMessage() {
        reset();
        payload.reserve(DEFAULT_PAYLOAD_SIZE);
    }
    
    void reset() {
        method_name.clear();
        payload.clear();
        request_id = 0;
        timeout_ms = 5000;
        is_response = false;
        error_code = 0;
        error_message.clear();
    }
    
    size_t estimateSize() const {
        return method_name.size() + payload.size() + error_message.size() + 
               sizeof(request_id) + sizeof(timeout_ms) + sizeof(is_response) + sizeof(error_code);
    }
};

/**
 * @brief 网络缓冲区结构
 */
struct NetworkBuffer {
    std::vector<uint8_t> data;
    size_t read_pos;
    size_t write_pos;
    
    static constexpr size_t DEFAULT_BUFFER_SIZE = 8192;
    
    NetworkBuffer() {
        reset();
        data.reserve(DEFAULT_BUFFER_SIZE);
    }
    
    void reset() {
        data.clear();
        read_pos = 0;
        write_pos = 0;
    }
    
    void resize(size_t size) {
        if (size > data.capacity()) {
            data.reserve(size * 2); // 预留更多空间
        }
        data.resize(size);
    }
    
    size_t available() const {
        return write_pos - read_pos;
    }
    
    size_t capacity() const {
        return data.capacity();
    }
    
    uint8_t* writePtr() {
        return data.data() + write_pos;
    }
    
    const uint8_t* readPtr() const {
        return data.data() + read_pos;
    }
    
    void advance_write(size_t bytes) {
        write_pos = std::min(write_pos + bytes, data.size());
    }
    
    void advance_read(size_t bytes) {
        read_pos = std::min(read_pos + bytes, write_pos);
    }
    
    void compact() {
        if (read_pos > 0) {
            std::memmove(data.data(), data.data() + read_pos, available());
            write_pos -= read_pos;
            read_pos = 0;
        }
    }
};

/**
 * @brief RPC消息对象池
 */
class MessagePool {
public:
    using MessagePoolType = ObjectPool<RpcMessage>;
    using BufferPoolType = ObjectPool<NetworkBuffer>;
    
    static MessagePool& getInstance() {
        static MessagePool instance;
        return instance;
    }
    
    /**
     * @brief 获取RPC消息对象
     */
    MessagePoolType::PooledObject acquireMessage(uint32_t timeout_ms = 0) {
        return message_pool_.acquire(timeout_ms);
    }
    
    /**
     * @brief 获取网络缓冲区对象
     */
    BufferPoolType::PooledObject acquireBuffer(uint32_t timeout_ms = 0) {
        return buffer_pool_.acquire(timeout_ms);
    }
    
    /**
     * @brief 获取消息池统计信息
     */
    MessagePoolType::Statistics getMessageStats() const {
        return message_pool_.getStatistics();
    }
    
    /**
     * @brief 获取缓冲区池统计信息
     */
    BufferPoolType::Statistics getBufferStats() const {
        return buffer_pool_.getStatistics();
    }
    
    /**
     * @brief 配置消息池
     */
    void configureMessagePool(const ObjectPool<RpcMessage>::Config& config) {
        // 注意：这里需要重新创建池，因为配置不能动态修改
        // 在实际应用中，应该在启动时配置
    }
    
    /**
     * @brief 配置缓冲区池
     */
    void configureBufferPool(const ObjectPool<NetworkBuffer>::Config& config) {
        // 注意：这里需要重新创建池，因为配置不能动态修改
        // 在实际应用中，应该在启动时配置
    }
    
    /**
     * @brief 打印统计信息
     */
    void printStatistics() const {
        auto msg_stats = getMessageStats();
        auto buf_stats = getBufferStats();
        
        printf("=== Message Pool Statistics ===\n");
        printf("Total Created: %lu\n", msg_stats.total_created.load());
        printf("Total Acquired: %lu\n", msg_stats.total_acquired.load());
        printf("Total Returned: %lu\n", msg_stats.total_returned.load());
        printf("Cache Hits: %lu\n", msg_stats.cache_hits.load());
        printf("Cache Misses: %lu\n", msg_stats.cache_misses.load());
        printf("Current Size: %lu\n", msg_stats.current_size.load());
        printf("Active Objects: %lu\n", msg_stats.active_objects.load());
        
        printf("\n=== Buffer Pool Statistics ===\n");
        printf("Total Created: %lu\n", buf_stats.total_created.load());
        printf("Total Acquired: %lu\n", buf_stats.total_acquired.load());
        printf("Total Returned: %lu\n", buf_stats.total_returned.load());
        printf("Cache Hits: %lu\n", buf_stats.cache_hits.load());
        printf("Cache Misses: %lu\n", buf_stats.cache_misses.load());
        printf("Current Size: %lu\n", buf_stats.current_size.load());
        printf("Active Objects: %lu\n", buf_stats.active_objects.load());
        
        // 计算命中率
        auto msg_total = msg_stats.cache_hits.load() + msg_stats.cache_misses.load();
        auto buf_total = buf_stats.cache_hits.load() + buf_stats.cache_misses.load();
        
        if (msg_total > 0) {
            printf("\nMessage Pool Hit Rate: %.2f%%\n", 
                   (double)msg_stats.cache_hits.load() / msg_total * 100.0);
        }
        
        if (buf_total > 0) {
            printf("Buffer Pool Hit Rate: %.2f%%\n", 
                   (double)buf_stats.cache_hits.load() / buf_total * 100.0);
        }
    }

private:
    MessagePool() 
        : message_pool_(
            []() { return std::make_unique<RpcMessage>(); },
            [](RpcMessage* msg) { msg->reset(); },
            {20, 200, 300000, true, true}  // 初始20个，最大200个
          )
        , buffer_pool_(
            []() { return std::make_unique<NetworkBuffer>(); },
            [](NetworkBuffer* buf) { buf->reset(); },
            {10, 100, 300000, true, true}  // 初始10个，最大100个
          ) {}
    
    ~MessagePool() = default;
    
    // 禁用拷贝和移动
    MessagePool(const MessagePool&) = delete;
    MessagePool& operator=(const MessagePool&) = delete;
    MessagePool(MessagePool&&) = delete;
    MessagePool& operator=(MessagePool&&) = delete;

private:
    MessagePoolType message_pool_;
    BufferPoolType buffer_pool_;
};

/**
 * @brief 便利函数：获取消息对象
 */
inline MessagePool::MessagePoolType::PooledObject acquireMessage(uint32_t timeout_ms = 0) {
    return MessagePool::getInstance().acquireMessage(timeout_ms);
}

/**
 * @brief 便利函数：获取缓冲区对象
 */
inline MessagePool::BufferPoolType::PooledObject acquireBuffer(uint32_t timeout_ms = 0) {
    return MessagePool::getInstance().acquireBuffer(timeout_ms);
}

} // namespace prpc

#endif // MESSAGE_POOL_H 