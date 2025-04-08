#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <thread>

namespace prpc {

/**
 * @brief 线程安全的通用对象池
 * @tparam T 对象类型
 */
template<typename T>
class ObjectPool {
public:
    using ObjectPtr = std::unique_ptr<T>;
    using FactoryFunc = std::function<ObjectPtr()>;
    using ResetFunc = std::function<void(T*)>;
    
    /**
     * @brief 对象池配置
     */
    struct Config {
        size_t initial_size = 10;          // 初始对象数量
        size_t max_size = 100;             // 最大对象数量
        size_t max_idle_time_ms = 300000;  // 最大空闲时间(5分钟)
        bool enable_validation = true;     // 启用对象验证
        bool enable_statistics = true;     // 启用统计信息
    };
    
    /**
     * @brief 对象池统计信息
     */
    struct Statistics {
        std::atomic<uint64_t> total_created{0};     // 总创建数
        std::atomic<uint64_t> total_acquired{0};    // 总获取数
        std::atomic<uint64_t> total_returned{0};    // 总归还数
        std::atomic<uint64_t> total_destroyed{0};   // 总销毁数
        std::atomic<uint64_t> cache_hits{0};        // 缓存命中数
        std::atomic<uint64_t> cache_misses{0};      // 缓存未命中数
        std::atomic<size_t> current_size{0};        // 当前池大小
        std::atomic<size_t> active_objects{0};      // 活跃对象数
        
        // 拷贝构造函数
        Statistics(const Statistics& other) 
            : total_created(other.total_created.load())
            , total_acquired(other.total_acquired.load())
            , total_returned(other.total_returned.load())
            , total_destroyed(other.total_destroyed.load())
            , cache_hits(other.cache_hits.load())
            , cache_misses(other.cache_misses.load())
            , current_size(other.current_size.load())
            , active_objects(other.active_objects.load()) {}
        
        // 赋值操作符
        Statistics& operator=(const Statistics& other) {
            if (this != &other) {
                total_created.store(other.total_created.load());
                total_acquired.store(other.total_acquired.load());
                total_returned.store(other.total_returned.load());
                total_destroyed.store(other.total_destroyed.load());
                cache_hits.store(other.cache_hits.load());
                cache_misses.store(other.cache_misses.load());
                current_size.store(other.current_size.load());
                active_objects.store(other.active_objects.load());
            }
            return *this;
        }
        
        // 默认构造函数
        Statistics() = default;
    };
    
    /**
     * @brief RAII对象包装器
     */
    class PooledObject {
    public:
        PooledObject(ObjectPtr obj, ObjectPool* pool) 
            : object_(std::move(obj)), pool_(pool) {}
        
        ~PooledObject() {
            if (object_ && pool_) {
                pool_->returnObject(std::move(object_));
            }
        }
        
        // 禁用拷贝
        PooledObject(const PooledObject&) = delete;
        PooledObject& operator=(const PooledObject&) = delete;
        
        // 启用移动
        PooledObject(PooledObject&& other) noexcept 
            : object_(std::move(other.object_)), pool_(other.pool_) {
            other.pool_ = nullptr;
        }
        
        PooledObject& operator=(PooledObject&& other) noexcept {
            if (this != &other) {
                if (object_ && pool_) {
                    pool_->returnObject(std::move(object_));
                }
                object_ = std::move(other.object_);
                pool_ = other.pool_;
                other.pool_ = nullptr;
            }
            return *this;
        }
        
        T* get() const { return object_.get(); }
        T& operator*() const { return *object_; }
        T* operator->() const { return object_.get(); }
        explicit operator bool() const { return object_ != nullptr; }
        
    private:
        ObjectPtr object_;
        ObjectPool* pool_;
    };
    
public:
    /**
     * @brief 构造对象池
     * @param factory 对象工厂函数
     * @param reset 对象重置函数(可选)
     * @param config 配置参数
     */
    ObjectPool(FactoryFunc factory, ResetFunc reset = nullptr, const Config& config = Config{})
        : factory_(std::move(factory))
        , reset_(std::move(reset))
        , config_(config)
        , shutdown_(false) {
        
        // 预创建初始对象
        for (size_t i = 0; i < config_.initial_size; ++i) {
            auto obj = createObject();
            if (obj) {
                available_.push({std::move(obj), std::chrono::steady_clock::now()});
                stats_.current_size.fetch_add(1);
            }
        }
        
        // 启动清理线程
        if (config_.max_idle_time_ms > 0) {
            cleanup_thread_ = std::thread(&ObjectPool::cleanupLoop, this);
        }
    }
    
    /**
     * @brief 析构函数
     */
    ~ObjectPool() {
        shutdown();
    }
    
    /**
     * @brief 获取对象
     * @param timeout_ms 超时时间(毫秒)，0表示不等待
     * @return RAII对象包装器
     */
    PooledObject acquire(uint32_t timeout_ms = 0) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 尝试从池中获取
        if (!available_.empty()) {
            auto item = std::move(available_.front());
            available_.pop();
            lock.unlock();
            
            auto obj = std::move(item.first);
            if (reset_ && obj) {
                reset_(obj.get());
            }
            
            stats_.total_acquired.fetch_add(1);
            stats_.cache_hits.fetch_add(1);
            stats_.active_objects.fetch_add(1);
            stats_.current_size.fetch_sub(1);
            
            return PooledObject(std::move(obj), this);
        }
        
        // 池为空，检查是否可以创建新对象
        if (stats_.current_size.load() + stats_.active_objects.load() >= config_.max_size) {
            if (timeout_ms == 0) {
                lock.unlock();
                stats_.cache_misses.fetch_add(1);
                return PooledObject(nullptr, this);
            }
            
            // 等待对象归还
            auto deadline = std::chrono::steady_clock::now() + 
                           std::chrono::milliseconds(timeout_ms);
            
            if (condition_.wait_until(lock, deadline, [this] { return !available_.empty() || shutdown_; })) {
                if (!available_.empty()) {
                    auto item = std::move(available_.front());
                    available_.pop();
                    lock.unlock();
                    
                    auto obj = std::move(item.first);
                    if (reset_ && obj) {
                        reset_(obj.get());
                    }
                    
                    stats_.total_acquired.fetch_add(1);
                    stats_.cache_hits.fetch_add(1);
                    stats_.active_objects.fetch_add(1);
                    stats_.current_size.fetch_sub(1);
                    
                    return PooledObject(std::move(obj), this);
                }
            }
            
            lock.unlock();
            stats_.cache_misses.fetch_add(1);
            return PooledObject(nullptr, this);
        }
        
        lock.unlock();
        
        // 创建新对象
        auto obj = createObject();
        if (obj) {
            stats_.total_acquired.fetch_add(1);
            stats_.cache_misses.fetch_add(1);
            stats_.active_objects.fetch_add(1);
        }
        
        return PooledObject(std::move(obj), this);
    }
    
    /**
     * @brief 获取统计信息
     */
    Statistics getStatistics() const {
        return stats_;
    }
    
    /**
     * @brief 清空对象池
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!available_.empty()) {
            available_.pop();
            stats_.total_destroyed.fetch_add(1);
        }
        stats_.current_size.store(0);
    }
    
    /**
     * @brief 获取当前池大小
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.size();
    }
    
    /**
     * @brief 检查池是否为空
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.empty();
    }
    
    /**
     * @brief 关闭对象池
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        condition_.notify_all();
        
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
        
        clear();
    }

private:
    /**
     * @brief 归还对象到池中
     */
    void returnObject(ObjectPtr obj) {
        if (!obj) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (shutdown_) {
            stats_.total_destroyed.fetch_add(1);
            stats_.active_objects.fetch_sub(1);
            return;
        }
        
        // 验证对象(如果启用)
        if (config_.enable_validation && !validateObject(obj.get())) {
            stats_.total_destroyed.fetch_add(1);
            stats_.active_objects.fetch_sub(1);
            return;
        }
        
        // 检查池大小限制
        if (available_.size() >= config_.max_size) {
            stats_.total_destroyed.fetch_add(1);
            stats_.active_objects.fetch_sub(1);
            return;
        }
        
        available_.push({std::move(obj), std::chrono::steady_clock::now()});
        stats_.total_returned.fetch_add(1);
        stats_.active_objects.fetch_sub(1);
        stats_.current_size.fetch_add(1);
        
        condition_.notify_one();
    }
    
    /**
     * @brief 创建新对象
     */
    ObjectPtr createObject() {
        try {
            auto obj = factory_();
            if (obj) {
                stats_.total_created.fetch_add(1);
            }
            return obj;
        } catch (...) {
            return nullptr;
        }
    }
    
    /**
     * @brief 验证对象有效性
     */
    bool validateObject(T* obj) {
        // 默认验证：检查对象是否为空
        return obj != nullptr;
    }
    
    /**
     * @brief 清理循环
     */
    void cleanupLoop() {
        while (!shutdown_) {
            std::this_thread::sleep_for(std::chrono::seconds(30)); // 每30秒清理一次
            
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_) break;
            
            auto now = std::chrono::steady_clock::now();
            auto max_idle = std::chrono::milliseconds(config_.max_idle_time_ms);
            
            // 清理过期对象
            std::queue<std::pair<ObjectPtr, std::chrono::steady_clock::time_point>> temp;
            
            while (!available_.empty()) {
                auto& item = available_.front();
                if (now - item.second > max_idle) {
                    available_.pop();
                    stats_.total_destroyed.fetch_add(1);
                    stats_.current_size.fetch_sub(1);
                } else {
                    temp.push(std::move(item));
                    available_.pop();
                }
            }
            
            available_ = std::move(temp);
        }
    }

private:
    FactoryFunc factory_;
    ResetFunc reset_;
    Config config_;
    
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::pair<ObjectPtr, std::chrono::steady_clock::time_point>> available_;
    
    std::atomic<bool> shutdown_;
    std::thread cleanup_thread_;
    
    mutable Statistics stats_;
};

} // namespace prpc

#endif // OBJECT_POOL_H 