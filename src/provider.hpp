#ifndef _Pprovider_HPP
#define _Pprovider_HPP

#include <google/protobuf/service.h>

#include "zookeeperutil.hpp"

class Pprovider {
 public:
  void NotifyService(google::protobuf::Service* servuce);
  ~Pprovider();
  void Run();

 private:
  struct ServiceInfo {
    google::protobuf::Service* m_service;
    std::unordered_map<std::string, const google::protobuf::MethodDescriptor*>
        m_methodMap;
  };
  std::unordered_map<std::string, ServiceInfo>
      m_serviceMap;  // 保存服务对象和rpc方法

  void HandleClientRequest(int clientfd);
};

class LambdaClosure : public google::protobuf::Closure {
public:
    explicit LambdaClosure(std::function<void()> func) : func_(std::move(func)) {}
    ~LambdaClosure() {}

    void Run() override {
        func_();
        // The closure is responsible for deleting itself after running.
        delete this;
    }

private:
    std::function<void()> func_;
};

#endif