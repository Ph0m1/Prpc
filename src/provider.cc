#include "provider.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <vector>

#include "application.h"
#include "header.pb.h"
#include "logger.h"
#include "threadpool.h"
#include "zookeeperutil.h"

// Constructor definition
Pprovider::Pprovider()
    : m_threadPool(std::make_unique<ThreadPool>()),
      m_zkClient(std::make_unique<ZkClient>()) {}

// Destructor definition - THIS IS IMPORTANT
Pprovider::~Pprovider() {}

void Pprovider::NotifyService(google::protobuf::Service *service) {
  ServiceInfo service_info;

  const google::protobuf::ServiceDescriptor *pserviceDesc =
      service->GetDescriptor();
  std::string service_name(pserviceDesc->name());
  int method_cnt = pserviceDesc->method_count();

  LOG(INFO) << "service_name: " << service_name;

  for (int i = 0; i < method_cnt; ++i) {
    const google::protobuf::MethodDescriptor *pmethodDesc =
        pserviceDesc->method(i);
    std::string method_name(pmethodDesc->name());
    service_info.m_methodMap.insert({method_name, pmethodDesc});
    LOG(INFO) << "method_name: " << method_name;
  }
  service_info.m_service = service;
  m_serviceMap.insert({service_name, service_info});
}
void Pprovider::RegisterServices() {
  std::string ip = Papplication::GetInstance().GetConfig().Load("rpcserverip");
  uint16_t port = atoi(
      Papplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
  for (auto &sp : m_serviceMap) {
    std::string service_path = "/" + sp.first;
    m_zkClient->Create(service_path.c_str(), nullptr, 0);
    for (auto &mp : sp.second.m_methodMap) {
      std::string method_path = service_path + "/" + mp.first;
      char method_path_data[128] = {0};
      sprintf(method_path_data, "%s:%d", ip.c_str(), port);
      m_zkClient->Create(method_path.c_str(), method_path_data,
                         strlen(method_path_data), ZOO_EPHEMERAL);
    }
  }
}

void Pprovider::OnZkSessionExpired() {
  LOG(ERROR)
      << "ZK session expired, re-connecting and re-registering services...";
  m_zkClient->Start(std::bind(&Pprovider::OnZkSessionExpired, this));
  RegisterServices();
}

void Pprovider::Run() {
  std::string ip = Papplication::GetInstance().GetConfig().Load("rpcserverip");
  uint16_t port = atoi(
      Papplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    LOG(FATAL) << "create socket error!";
  }

  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    LOG(FATAL) << "bind error!";
  }

  if (listen(listenfd, 20) == -1) {
    LOG(FATAL) << "listen error!";
  }
  LOG(INFO) << "Rpc provider start service at ip:" << ip << " port:" << port;

  m_zkClient->Start(std::bind(&Pprovider::OnZkSessionExpired, this));
  RegisterServices();

  int epollfd = epoll_create1(0);
  epoll_event events[1024];
  epoll_event event;
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = listenfd;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);

  while (true) {
    int nfds = epoll_wait(epollfd, events, 1024, -1);
    if (nfds == -1) {
      if (errno == EINTR) continue;
      LOG(ERROR) << "epoll_wait error";
      break;
    }

    for (int i = 0; i < nfds; ++i) {
      int sockfd = events[i].data.fd;
      if (sockfd == listenfd) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int connfd =
            accept(listenfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (connfd < 0) {
          LOG(ERROR) << "accept error";
          continue;
        }
        LOG(INFO) << "new connection accepted.";

        event.data.fd = connfd;
        event.events = EPOLLIN | EPOLLET;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &event);
      } else if (events[i].events & EPOLLIN) {
        m_threadPool->submit(
            std::bind(&Pprovider::HandleClientRequest, this, sockfd));
      }
    }
  }
  close(listenfd);
  close(epollfd);
}

void Pprovider::HandleClientRequest(int clientfd) {
  int header_size = 0;
  int n = recv(clientfd, &header_size, 4, 0);
  if (n <= 0) {
    close(clientfd);
    return;
  }

  std::string rpc_header_str(header_size, '\0');
  n = recv(clientfd, &rpc_header_str[0], header_size, 0);
  if (n <= 0) {
    close(clientfd);
    return;
  }

  Prpc::RpcHeader rpcHeader;
  if (!rpcHeader.ParseFromString(rpc_header_str)) {
    LOG(ERROR) << "rpc_header_str parse error!";
    close(clientfd);
    return;
  }

  std::string service_name = rpcHeader.service_name();
  std::string method_name = rpcHeader.method_name();
  uint32_t args_size = rpcHeader.args_size();

  std::string args_str(args_size, '\0');
  n = recv(clientfd, &args_str[0], args_size, 0);
  if (n <= 0) {
    close(clientfd);
    return;
  }

  auto sit = m_serviceMap.find(service_name);
  if (sit == m_serviceMap.end()) {
    LOG(ERROR) << service_name << " is not exist!";
    close(clientfd);
    return;
  }
  auto mit = sit->second.m_methodMap.find(method_name);
  if (mit == sit->second.m_methodMap.end()) {
    LOG(ERROR) << service_name << ":" << method_name << " is not exist!";
    close(clientfd);
    return;
  }

  google::protobuf::Service *service = sit->second.m_service;
  const google::protobuf::MethodDescriptor *methodDesc = mit->second;

  google::protobuf::Message *request =
      service->GetRequestPrototype(methodDesc).New();
  if (!request->ParseFromString(args_str)) {
    LOG(ERROR) << "request parse error, content:" << args_str;
    delete request;
    close(clientfd);
    return;
  }
  google::protobuf::Message *response =
      service->GetResponsePrototype(methodDesc).New();

  google::protobuf::Closure *done = new LambdaClosure([this, clientfd, request,
                                                       response]() {
    std::string response_str;
    if (response->SerializeToString(&response_str)) {
      if (send(clientfd, response_str.c_str(), response_str.size(), 0) < 0) {
        LOG(ERROR) << "send response error!";
      }
    } else {
      LOG(ERROR) << "serialize response error!";
    }
    delete request;
    delete response;
  });

  service->CallMethod(methodDesc, nullptr, request, response, done);
}
