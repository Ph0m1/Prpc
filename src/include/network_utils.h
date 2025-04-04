#ifndef PRPC_NETWORK_UTILS_H
#define PRPC_NETWORK_UTILS_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <string>
#include "error.h"

namespace prpc {
namespace network {

// RAII Socket包装器
class Socket {
public:
    Socket() : fd_(-1) {}
    
    explicit Socket(int domain, int type, int protocol = 0) : fd_(-1) {
        fd_ = socket(domain, type, protocol);
        if (fd_ == -1) {
            throw NetworkException("Failed to create socket: " + std::string(std::strerror(errno)));
        }
    }
    
    ~Socket() {
        if (fd_ != -1) {
            close(fd_);
        }
    }
    
    // 禁用拷贝
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    // 允许移动
    Socket(Socket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (fd_ != -1) {
                close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    // 获取文件描述符
    int get() const { return fd_; }
    
    // 检查socket是否有效
    bool isValid() const { return fd_ != -1; }
    
    // 设置socket选项
    void setOption(int level, int optname, const void* optval, socklen_t optlen) {
        if (setsockopt(fd_, level, optname, optval, optlen) == -1) {
            throw NetworkException("Failed to set socket option: " + std::string(std::strerror(errno)));
        }
    }
    
    // 绑定地址
    void bind(const struct sockaddr* addr, socklen_t addrlen) {
        if (::bind(fd_, addr, addrlen) == -1) {
            throw NetworkException("Failed to bind socket: " + std::string(std::strerror(errno)));
        }
    }
    
    // 监听连接
    void listen(int backlog) {
        if (::listen(fd_, backlog) == -1) {
            throw NetworkException("Failed to listen on socket: " + std::string(std::strerror(errno)));
        }
    }
    
    // 接受连接
    Socket accept(struct sockaddr* addr = nullptr, socklen_t* addrlen = nullptr) {
        int client_fd = ::accept(fd_, addr, addrlen);
        if (client_fd == -1) {
            throw NetworkException("Failed to accept connection: " + std::string(std::strerror(errno)));
        }
        return Socket(client_fd);
    }
    
    // 连接到服务器
    void connect(const struct sockaddr* addr, socklen_t addrlen) {
        if (::connect(fd_, addr, addrlen) == -1) {
            throw NetworkException("Failed to connect: " + std::string(std::strerror(errno)));
        }
    }
    
    // 发送数据
    ssize_t send(const void* buf, size_t len, int flags = 0) {
        ssize_t result = ::send(fd_, buf, len, flags);
        if (result == -1) {
            throw NetworkException("Failed to send data: " + std::string(std::strerror(errno)));
        }
        return result;
    }
    
    // 接收数据
    ssize_t recv(void* buf, size_t len, int flags = 0) {
        ssize_t result = ::recv(fd_, buf, len, flags);
        if (result == -1) {
            throw NetworkException("Failed to receive data: " + std::string(std::strerror(errno)));
        }
        return result;
    }
    
    // 设置超时
    void setTimeout(int timeout_ms) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setOption(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setOption(SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
    
    // 设置非阻塞模式
    void setNonBlocking() {
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags == -1) {
            throw NetworkException("Failed to get socket flags: " + std::string(std::strerror(errno)));
        }
        if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            throw NetworkException("Failed to set non-blocking mode: " + std::string(std::strerror(errno)));
        }
    }
    
    // 设置地址重用
    void setReuseAddr() {
        int opt = 1;
        setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }
    
    // 设置保活
    void setKeepAlive() {
        int opt = 1;
        setOption(SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    }

private:
    explicit Socket(int fd) : fd_(fd) {}
    int fd_;
};

// 地址工具类
class Address {
public:
    Address() {}
    
    Address(const std::string& ip, uint16_t port) {
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) != 1) {
            throw NetworkException("Invalid IP address: " + ip);
        }
    }
    
    const struct sockaddr_in& getSockAddr() const { return addr_; }
    struct sockaddr_in& getSockAddr() { return addr_; }
    
    std::string getIp() const {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr_.sin_addr, ip, INET_ADDRSTRLEN);
        return std::string(ip);
    }
    
    uint16_t getPort() const {
        return ntohs(addr_.sin_port);
    }
    
    socklen_t getSize() const {
        return sizeof(addr_);
    }
    
    const struct sockaddr* getSockAddrPtr() const {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }
    
    struct sockaddr* getSockAddrPtr() {
        return reinterpret_cast<struct sockaddr*>(&addr_);
    }

private:
    struct sockaddr_in addr_ = {};
};

// 网络工具函数
namespace utils {
    // 创建TCP服务器socket
    inline Socket createTcpServer(const std::string& ip, uint16_t port, int backlog = 20) {
        Socket socket(AF_INET, SOCK_STREAM);
        socket.setReuseAddr();
        
        Address addr(ip, port);
        socket.bind(addr.getSockAddrPtr(), addr.getSize());
        socket.listen(backlog);
        
        return socket;
    }
    
    // 创建TCP客户端socket
    inline Socket createTcpClient(const std::string& ip, uint16_t port) {
        Socket socket(AF_INET, SOCK_STREAM);
        
        Address addr(ip, port);
        socket.connect(addr.getSockAddrPtr(), addr.getSize());
        
        return socket;
    }
    
    // 安全发送数据
    inline Result<ssize_t> safeSend(Socket& socket, const void* data, size_t len) {
        return ErrorHandler::safeExecute([&]() {
            return socket.send(data, len);
        });
    }
    
    // 安全接收数据
    inline Result<ssize_t> safeRecv(Socket& socket, void* buffer, size_t len) {
        return ErrorHandler::safeExecute([&]() {
            return socket.recv(buffer, len);
        });
    }
}

} // namespace network
} // namespace prpc

#endif // PRPC_NETWORK_UTILS_H 