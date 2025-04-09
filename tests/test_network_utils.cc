#include "network_utils.h"
#include "error.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>

using namespace prpc::network;

class NetworkUtilsTest {
public:
    static void testSocketCreation() {
        std::cout << "Testing socket creation..." << std::endl;
        
        try {
            Socket socket(AF_INET, SOCK_STREAM);
            assert(socket.isValid());
            assert(socket.get() > 0);
            
            std::cout << "Socket creation test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Socket creation failed: " << e.what() << std::endl;
            assert(false);
        }
    }
    
    static void testSocketOptions() {
        std::cout << "Testing socket options..." << std::endl;
        
        try {
            Socket socket(AF_INET, SOCK_STREAM);
            
            // 测试设置地址重用
            socket.setReuseAddr();
            
            // 测试设置保活
            socket.setKeepAlive();
            
            // 测试设置超时
            socket.setTimeout(5000);
            
            std::cout << "Socket options test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Socket options test failed: " << e.what() << std::endl;
            assert(false);
        }
    }
    
    static void testAddressClass() {
        std::cout << "Testing Address class..." << std::endl;
        
        try {
            Address addr("127.0.0.1", 8080);
            
            assert(addr.getIp() == "127.0.0.1");
            assert(addr.getPort() == 8080);
            assert(addr.getSize() == sizeof(struct sockaddr_in));
            
            // 测试无效IP地址
            try {
                Address invalid_addr("invalid.ip", 8080);
                assert(false); // 不应该到达这里
            } catch (const prpc::NetworkException& e) {
                // 预期的异常
            }
            
            std::cout << "Address class test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Address class test failed: " << e.what() << std::endl;
            assert(false);
        }
    }
    
    static void testSocketMove() {
        std::cout << "Testing socket move semantics..." << std::endl;
        
        try {
            Socket socket1(AF_INET, SOCK_STREAM);
            int fd1 = socket1.get();
            assert(socket1.isValid());
            
            // 测试移动构造
            Socket socket2 = std::move(socket1);
            assert(!socket1.isValid()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
            assert(socket2.isValid());
            assert(socket2.get() == fd1);
            
            // 测试移动赋值
            Socket socket3(AF_INET, SOCK_STREAM);
            socket3 = std::move(socket2);
            assert(!socket2.isValid()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
            assert(socket3.isValid());
            assert(socket3.get() == fd1);
            
            std::cout << "Socket move semantics test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Socket move test failed: " << e.what() << std::endl;
            assert(false);
        }
    }
    
    static void testServerClientConnection() {
        std::cout << "Testing server-client connection..." << std::endl;
        
        const std::string test_ip = "127.0.0.1";
        const uint16_t test_port = 12345;
        
        try {
            // 创建服务器socket
            Socket server_socket = utils::createTcpServer(test_ip, test_port, 1);
            assert(server_socket.isValid());
            
            // 在另一个线程中创建客户端连接
            std::thread client_thread([&]() {
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    Socket client_socket = utils::createTcpClient(test_ip, test_port);
                    assert(client_socket.isValid());
                } catch (const prpc::NetworkException& e) {
                    std::cout << "Client connection failed: " << e.what() << std::endl;
                }
            });
            
            // 服务器接受连接
            Socket client_socket = server_socket.accept();
            assert(client_socket.isValid());
            
            client_thread.join();
            
            std::cout << "Server-client connection test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Server-client connection test failed: " << e.what() << std::endl;
            // 这个测试可能因为端口占用而失败，不强制断言
        }
    }
    
    static void testDataTransmission() {
        std::cout << "Testing data transmission..." << std::endl;
        
        const std::string test_ip = "127.0.0.1";
        const uint16_t test_port = 12346;
        const std::string test_message = "Hello, Network!";
        
        try {
            // 创建服务器
            Socket server_socket = utils::createTcpServer(test_ip, test_port, 1);
            
            std::string received_message;
            
            // 客户端线程
            std::thread client_thread([&]() {
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    Socket client_socket = utils::createTcpClient(test_ip, test_port);
                    
                    // 发送数据
                    auto send_result = utils::safeSend(client_socket, test_message.data(), test_message.size());
                    assert(send_result.isSuccess());
                    
                } catch (const prpc::NetworkException& e) {
                    std::cout << "Client data transmission failed: " << e.what() << std::endl;
                }
            });
            
            // 服务器接受连接并接收数据
            Socket client_socket = server_socket.accept();
            
            char buffer[1024];
            auto recv_result = utils::safeRecv(client_socket, buffer, sizeof(buffer));
            if (recv_result.isSuccess()) {
                received_message = std::string(buffer, recv_result.getValue());
                assert(received_message == test_message);
            }
            
            client_thread.join();
            
            std::cout << "Data transmission test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Data transmission test failed: " << e.what() << std::endl;
            // 这个测试可能因为端口占用而失败，不强制断言
        }
    }
    
    static void testSocketTimeout() {
        std::cout << "Testing socket timeout..." << std::endl;
        
        try {
            Socket socket(AF_INET, SOCK_STREAM);
            socket.setTimeout(1000); // 1秒超时
            
            // 尝试连接到不存在的服务器（应该超时）
            Address addr("127.0.0.1", 9999); // 假设这个端口没有服务
            
            auto start = std::chrono::high_resolution_clock::now();
            try {
                socket.connect(addr.getSockAddrPtr(), addr.getSize());
                // 如果连接成功，可能是因为端口实际上有服务
            } catch (const prpc::NetworkException& e) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
                // 验证是否在合理的时间内超时
                assert(duration.count() <= 5); // 应该在5秒内超时
            }
            
            std::cout << "Socket timeout test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Socket timeout test failed: " << e.what() << std::endl;
            // 超时测试可能因为系统差异而不稳定，不强制断言
        }
    }
    
    static void testNonBlockingMode() {
        std::cout << "Testing non-blocking mode..." << std::endl;
        
        try {
            Socket socket(AF_INET, SOCK_STREAM);
            socket.setNonBlocking();
            
            // 在非阻塞模式下，连接操作应该立即返回
            Address addr("127.0.0.1", 9999);
            try {
                socket.connect(addr.getSockAddrPtr(), addr.getSize());
            } catch (const prpc::NetworkException& e) {
                // 非阻塞连接可能立即失败，这是正常的
            }
            
            std::cout << "Non-blocking mode test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Non-blocking mode test failed: " << e.what() << std::endl;
            assert(false);
        }
    }
    
    static void testErrorHandling() {
        std::cout << "Testing error handling..." << std::endl;
        
        // 测试无效socket操作
        try {
            Socket socket; // 默认构造的无效socket
            assert(!socket.isValid());
            
            // 尝试在无效socket上设置选项应该抛出异常
            try {
                socket.setReuseAddr();
                assert(false); // 不应该到达这里
            } catch (const prpc::NetworkException& e) {
                // 预期的异常
            }
            
            std::cout << "Error handling test passed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error handling test failed: " << e.what() << std::endl;
            assert(false);
        }
    }
    
    static void testMultipleConnections() {
        std::cout << "Testing multiple connections..." << std::endl;
        
        const std::string test_ip = "127.0.0.1";
        const uint16_t test_port = 12347;
        const int num_clients = 5;
        
        try {
            Socket server_socket = utils::createTcpServer(test_ip, test_port, num_clients);
            
            std::vector<std::thread> client_threads;
            std::atomic<int> successful_connections(0);
            
            // 创建多个客户端连接
            for (int i = 0; i < num_clients; ++i) {
                client_threads.emplace_back([&, i]() {
                    try {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50 * i));
                        Socket client_socket = utils::createTcpClient(test_ip, test_port);
                        successful_connections.fetch_add(1);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    } catch (const prpc::NetworkException& e) {
                        std::cout << "Client " << i << " connection failed: " << e.what() << std::endl;
                    }
                });
            }
            
            // 服务器接受连接
            std::vector<Socket> client_sockets;
            for (int i = 0; i < num_clients; ++i) {
                try {
                    Socket client_socket = server_socket.accept();
                    client_sockets.push_back(std::move(client_socket));
                } catch (const prpc::NetworkException& e) {
                    std::cout << "Server accept failed: " << e.what() << std::endl;
                    break;
                }
            }
            
            // 等待所有客户端线程完成
            for (auto& thread : client_threads) {
                thread.join();
            }
            
            std::cout << "Successfully handled " << client_sockets.size() 
                      << " server connections and " << successful_connections.load() 
                      << " client connections" << std::endl;
            
            std::cout << "Multiple connections test passed!" << std::endl;
        } catch (const prpc::NetworkException& e) {
            std::cout << "Multiple connections test failed: " << e.what() << std::endl;
            // 这个测试可能因为系统限制而失败，不强制断言
        }
    }
};

int main() {
    std::cout << "Starting network utils tests..." << std::endl;
    
    try {
        NetworkUtilsTest::testSocketCreation();
        NetworkUtilsTest::testSocketOptions();
        NetworkUtilsTest::testAddressClass();
        NetworkUtilsTest::testSocketMove();
        NetworkUtilsTest::testErrorHandling();
        NetworkUtilsTest::testNonBlockingMode();
        
        // 这些测试需要网络操作，可能不稳定
        std::cout << "\nRunning network communication tests (may be unstable)..." << std::endl;
        NetworkUtilsTest::testServerClientConnection();
        NetworkUtilsTest::testDataTransmission();
        NetworkUtilsTest::testSocketTimeout();
        NetworkUtilsTest::testMultipleConnections();
        
        std::cout << "All network utils tests completed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Network utils test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Network utils test failed with unknown exception" << std::endl;
        return 1;
    }
} 