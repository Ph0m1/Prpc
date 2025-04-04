#ifndef _Pchannel_H
#define _Pchannel_H
// From google::protobuf::RpcChannel
#include <google/protobuf/service.h>

#include "zookeeperutil.h"
class Pchannel : public google::protobuf::RpcChannel {
 public:
  Pchannel(bool connectNow);
  virtual ~Pchannel() {};
  void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                  ::google::protobuf::RpcController *controller,
                  const ::google::protobuf::Message *request,
                  ::google::protobuf::Message *response,
                  ::google::protobuf::Closure *done) override;

 private:
  int m_clientfd;
  std::string service_name;
  std::string m_ip;
  uint16_t m_port;
  std::string method_name;
  int m_idx;
  std::unordered_map<std::string, int> m_connections;
  std::mutex m_mutex;
  bool newConnect(const char *ip, uint16_t port);
  std::string QueryServiceHost(ZkClient *zkclient, std::string service_name,
                               std::string method_name, int &idx);
};
#endif