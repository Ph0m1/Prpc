#include "channel.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "header.pb.h"
#include "logger.hpp"
#include "zookeeperutil.hpp"

std::mutex g_data_mutx;

void Pchannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                          ::google::protobuf::RpcController *controller,
                          const ::google::protobuf::Message *request,
                          ::google::protobuf::Message *response,
                          ::google::protobuf::Closure *done) {
  if (-1 == m_clientfd) {
    const google::protobuf::ServiceDescriptor *sd = method->service();
    service_name = sd->name();
    method_name = method->name();

    ZkClient zkCli;
    zkCli.Start();
    std::string host_data =
        QueryServiceHost(&zkCli, service_name, method_name, m_idx);
    m_ip = host_data.substr(0, m_idx);
    LOG(INFO) << "ip: " << m_ip;
    m_port =
        atoi(host_data.substr(m_idx + 1, host_data.size() - m_idx).c_str());
    LOG(INFO) << "port: " << m_port;

    auto rt = newConnect(m_ip.c_str(), m_port);
    if (!rt) {
      LOG(ERROR) << "connect server error";
      return;
    } else {
      LOG(INFO) << "connect server success";
    }
  }

  uint32_t args_size{};
  std::string args_str;
  if (request->SerializeToString(&args_str)) {
    args_size = args_str.size();
  } else {
    controller->SetFailed("serialize request fail");
    return;
  }

  // header info and settings
  Prpc::RpcHeader prpcheader;
  prpcheader.set_service_name(service_name);
  prpcheader.set_method_name(method_name);
  prpcheader.set_args_size(args_size);

  // make header to string
  uint32_t header_size = 0;
  std::string rpc_header_str;
  if (prpcheader.SerializePartialToString(&rpc_header_str)) {
    header_size = rpc_header_str.size();
  } else {
    controller->SetFailed("serialize rpc header error!");
    return;
  }

  // make RPC request message
  std::string send_rpc_str;
  {
    google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
    google::protobuf::io::CodedOutputStream coded_output(&string_output);
    coded_output.WriteVarint32(static_cast<uint32_t>(header_size));
    coded_output.WriteString(rpc_header_str);
  }
  send_rpc_str += args_str;

  // send RPC request
  if (-1 == send(m_clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0)) {
    close(m_clientfd);
    char errtxt[512] = {};
    LOG(ERROR) << "send error: " << strerror_r(errno, errtxt, sizeof(errtxt));
    controller->SetFailed(errtxt);
    return;
  }

  // response received
  char recv_buf[1024] = {0};
  int recv_size = 0;
  if (-1 == (recv_size = recv(m_clientfd, recv_buf, 1024, 0))) {
    close(m_clientfd);
    char errtxt[512] = {};
    LOG(ERROR) << "parse error" << strerror_r(errno, errtxt, sizeof(errtxt));
    controller->SetFailed(errtxt);
    return;
  }

  // deserialize into response object
  if (!response->ParseFromArray(recv_buf, recv_size)) {
    close(m_clientfd);
    char errtxt[512] = {};
    LOG(ERROR) << "parse error" << strerror_r(errno, errtxt, sizeof(errtxt));
    controller->SetFailed(errtxt);
    return;
  }

  close(m_clientfd);
}

bool Pchannel::newConnect(const char *ip, uint16_t port) {
  // create scoket
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == clientfd) {
    char errtxt[512] = {0};
    std::cout << "socket error" << strerror_r(errno, errtxt, sizeof(errtxt))
              << std::endl;
    LOG(ERROR) << "socket error:" << errtxt;
    return false;
  }

  // set service info
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  // try to connect server
  if (-1 ==
      connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
    close(clientfd);
    char errtxt[512] = {0};
    std::cout << "connect error" << strerror_r(errno, errtxt, sizeof(errtxt))
              << std::endl;
    LOG(ERROR) << "connect server error" << errtxt;
    return false;
  }

  m_clientfd = clientfd;
  return true;
}

// reserch service address from zookeeper
std::string Pchannel::QueryServiceHost(ZkClient *zkclient,
                                       std::string service_name,
                                       std::string method_name, int &idx) {
  std::string method_path = "/" + service_name + "/" + method_name;
  std::cout << "method_path: " << method_path << std::endl;

  std::unique_lock<std::mutex> lock(g_data_mutx);
  std::string host_data_1 =
      zkclient->GetData(method_path.c_str());  // read data from zookeeper
  lock.unlock();

  if (host_data_1 == "") {
    LOG(ERROR) << method_path << "is not exist!";
    return " ";
  }

  idx = host_data_1.find(":");
  if (idx == -1) {
    LOG(ERROR) << method_path << " address is invalid!";
    return " ";
  }

  return host_data_1;
}

// support delayed connection
Pchannel::Pchannel(bool connectNow) : m_clientfd(-1), m_idx(0) {
  if (!connectNow) {
    return;
  }

  // test for 3 times
  auto rt = newConnect(m_ip.c_str(), m_port);
  int count = 3;
  while (!rt && count--) {
    rt = newConnect(m_ip.c_str(), m_port);
  }
}