#include "channel.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "controller.h"
#include "header.pb.h"
#include "logger.h"
#include "zookeeperutil.h"

std::mutex g_data_mutx;

void Pchannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) {
  const google::protobuf::ServiceDescriptor *sd = method->service();
  std::string service_name(sd->name());
  std::string method_name(method->name());

  std::string args_str;
  if (!request->SerializeToString(&args_str)) {
    controller->SetFailed("serialize request error!");
    return;
  }

  Prpc::RpcHeader rpcHeader;
  rpcHeader.set_service_name(service_name);
  rpcHeader.set_method_name(method_name);
  rpcHeader.set_args_size(args_str.size());

  std::string rpc_header_str;
  if (!rpcHeader.SerializeToString(&rpc_header_str)) {
    controller->SetFailed("serialize rpc header error!");
    return;
  }

  uint32_t header_size = rpc_header_str.size();
  std::string send_rpc_str;
  send_rpc_str.append((char *)&header_size, 4);
  send_rpc_str += rpc_header_str;
  send_rpc_str += args_str;

  ZkClient zkCli;
  zkCli.Start();
  std::string method_path = "/" + service_name + "/" + method_name;
  std::string host_data = zkCli.GetData(method_path.c_str());
  if (host_data.empty()) {
    controller->SetFailed(method_path + " is not exist!");
    return;
  }

  size_t idx = host_data.find(":");
  if (idx == std::string::npos) {
    controller->SetFailed(method_path + " address is invalid!");
    return;
  }
  std::string ip = host_data.substr(0, idx);
  uint16_t port = atoi(host_data.substr(idx + 1).c_str());

  int clientfd = -1;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connections.count(host_data)) {
      clientfd = m_connections[host_data];
    }
  }

  if (clientfd == -1) {
    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
      controller->SetFailed("create socket error!");
      return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(clientfd, (sockaddr *)&server_addr, sizeof(server_addr)) ==
        -1) {
      controller->SetFailed("connect error!");
      close(clientfd);
      return;
    }

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_connections[host_data] = clientfd;
    }
  }

  Pcontroller *p_controller = dynamic_cast<Pcontroller *>(controller);
  if (p_controller) {
    int timeout_ms = p_controller->GetTimeout();
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = static_cast<__suseconds_t>((timeout_ms % 1000) * 1000);
    setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }

  if (send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1) {
    controller->SetFailed("send error!");
    close(clientfd);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(host_data);
    return;
  }

  char recv_buf[1024] = {0};
  int recv_size = recv(clientfd, recv_buf, sizeof(recv_buf), 0);
  if (recv_size <= 0) {
    if (recv_size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      controller->SetFailed("recv timeout!");
    } else {
      controller->SetFailed("recv error!");
    }
    close(clientfd);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(host_data);
    return;
  }

  if (!response->ParseFromArray(recv_buf, recv_size)) {
    controller->SetFailed("parse error!");
    close(clientfd);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(host_data);
    return;
  }

  if (done != nullptr) {
    done->Run();
  }
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