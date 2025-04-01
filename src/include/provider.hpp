#ifndef _Pprovider_HPP
#define _Pprovider_HPP

#include <google/protobuf/service.h>

#include "zookeeperutil.hpp"

class ThreadPool;

struct ServiceInfo {
  google::protobuf::Service* m_service;
  std::unordered_map<std::string, const google::protobuf::MethodDescriptor*>
      m_methodMap;
};

class Pprovider {
 public:
  Pprovider();
  ~Pprovider();

  void NotifyService(google::protobuf::Service* servuce);
  void Run();

 private:
  void RegisterServices();
  void OnZkSessionExpired();
  void HandleClientRequest(int clientfd);

  std::unordered_map<std::string, ServiceInfo>
      m_serviceMap;  // 保存服务对象和rpc方法
  std::unique_ptr<ThreadPool> m_threadPool;
  std::unique_ptr<ZkClient> m_zkClient;
};

class LambdaClosure : public google::protobuf::Closure {
 public:
  explicit LambdaClosure(std::function<void()> func) : func_(std::move(func)) {}
  ~LambdaClosure() {}

  void Run() override {
    if (func_) {
      func_();
    }
    // The closure is responsible for deleting itself after running.
    delete this;
  }

 private:
  std::function<void()> func_;
};

#endif