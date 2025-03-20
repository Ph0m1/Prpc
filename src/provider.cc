#include "provider.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <vector>

#include "application.hpp"
#include "header.pb.h"
#include "logger.hpp"
#include "threadpool.hpp"
#include "zookeeperutil.hpp"

void Pprovider::NotifyService(google::protobuf::Service* service) {
  ServiceInfo service_info;

  // got service info
  const google::protobuf::ServiceDescriptor* pserviceDesc =
      service->GetDescriptor();
  // get service name
  std::string service_name = pserviceDesc->name();
  int methodCnt = pserviceDesc->method_count();

  LOG(INFO) << "service_name: " << service_name;

  for (int i = 0; i < methodCnt; ++i) {
    const google::protobuf::MethodDescriptor* pmethodDesc =
        pserviceDesc->Method(i);
    std::string method_name = pmethodDesc->name();
    service_info.m_methodMap.insert({method_name, pmethodDesc});

    LOG(INFO) << "method_name: " << method_name;
  }
  service_info.m_service = service;
  m_serviceMap.insert({service_name, service_info});
}

void Pprovider::Run() {
  // get settings
  std::string ip = Papplication::GetInstance().GetConfig().Load("pcserverip");
  uint16_t port = atoi(Papplication::GetInstance()
                           .GetInstance()
                           .GetConfig()
                           .Load("repcserverport")
                           .c_str());
  int thread_num = atoi(Papplication::GetInstance()
                            .GetInstance()
                            .GetConfig()
                            .Load("threadnum")
                            .c_str());

  ThreadPool threadpool(thread_num);

  // create listening socket
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    LOG(ERROR) << "create socket error! errno: " << errno;
    exit(EXIT_FAILURE);
  }

  // port reuse
  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

  // bind
  if (bind(listenfd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    LOG(ERROR) << "bind error! errno: " << errno;
    exit(EXIT_FAILURE);
  }

  // listen
  if (listen(listenfd, SOMAXCONN) == -1) {
    LOG(ERROR) << "listen error! errno: " << errno;
    exit(EXIT_FAILURE);
  }

  // create epoll
  int epollfd = epoll_create1(0);
  if (epollfd == -1) {
    LOG(ERROR) << "epoll_create1 error! errno: " << errno;
    exit(EXIT_FAILURE);
  }

  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = listenfd;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);

  LOG(INFO) << "RpcProvider start service at ip: " << ip << " port: " << port;

  // register all services to be published on zk

  ZkClient zkCli;
  zkCli.Start();
  for (auto& sp : m_serviceMap) {
    std::string service_path = "/" + sp.first;
    zkCli.Create(service_path.c_str(), nullptr, 0);
    for (auto& mp : sp.second.m_methodMap) {
      std::string method_path = service_path + "/" + mp.first;
      char method_path_data[128] = {0};
      sprintf(method_path_data, "%s:%d", ip.c_str(), port);
      // ZOO_EPHEMERAL indicates that the znode is a temporary node
      zkCli.Create(method_path.c_str(), method_path_data,
                   strlen(method_path_data), ZOO_EPHEMERAL);
    }
  }

  // epoll_wait
  std::vector<epoll_event> ready_events(1024);
  while (true) {
    int num_events =
        epoll_wait(epollfd, &ready_events[0], ready_events.size(), -1);
    if (num_events == -1) {
      if (errno == EINTR) {
        continue;
      }
      LOG(ERROR) << "epoll_wait error! errno: " << errno;
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_events; ++i) {
      int sockfd = ready_events[i].data.fd;
      if (sockfd == listenfd) {
        // new client
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int clientfd =
            accept(listenfd, (sockaddr*)&client_addr, &client_addr_len);
        if (clientfd == -1) {
          LOG(ERROR) << "accept error! errno: " << errno;
          continue;
        }

        // listening
        epoll_event client_event;
        client_event.events = EPOLLIN;
        client_event.data.fd = clientfd;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &client_event);
        LOG(INFO) << "New connection, client fd: " << clientfd;
      } else {
        if (ready_events[i].events & EPOLLIN) {
          threadpool.enqueue(
              std::bind(&Pprovider::HandleClientRequest, this, sockfd));
        }
      }
    }
  }
}

void Pprovider::HandleClientRequest(int clientfd) {
  char header_size_buf[4];
  int n = recv(clientfd, header_size_buf, 4, 0);
  if (n == 0) {
    LOG(INFO) << "client fd: " << clientfd << " disconnected.";
    close(clientfd);
    return;
  } else if (n < 0) {
    LOG(ERROR) << "recv header size error, fd: " << clientfd
               << " errno: " << errno;
    close(clientfd);
    return;
  }

  uint32_t header_size = 0;
  memcpy(&header_size, header_size_buf, 4);

  // read RPC header
  std::string rpc_header_str(header_size, '\0');
  n = recv(clientfd, &rpc_header_str[0], header_size, 0);
  if (n <= 0) {
    LOG(ERROR) << "recv header error, fd: " << clientfd << " errno: " << errno;
    close(clientfd);
    return;
  }

  //
  Prpc::RpcHeader rpcHeader;
  std::string service_name;
  std::string method_name;
  uint32_t args_size;
  if (rpcHeader.ParseFromString(rpc_header_str)) {
    service_name = rpcHeader.service_name();
    method_name = rpcHeader.method_name();
    args_size = rpcHeader.args_size();
  } else {
    LOG(ERROR) << "rpc_header_str: " << rpc_header_str << " parse error!";
    close(clientfd);
    return;
  }

  // read rpc_args
  std::string args_str(args_size, '\0');
  n = recv(clientfd, &args_str[0], args_size, 0);
  if (n <= 0) {
    LOG(ERROR) << "recv args error, fd: " << clientfd << " errno: " << errno;
    close(clientfd);
    return;
  }

  // get service and method
  auto it = m_serviceMap.find(service_name);
  if (it == m_serviceMap.end()) {
    LOG(ERROR) << service_name << " is not a valid service!";
    close(clientfd);
    return;
  }

  auto mit = it->second.m_methodMap.find(method_name);
  if (mit == it->second.m_methodMap.end()) {
    LOG(ERROR) << service_name << ":" << method_name
               << " is not a calid method!";
    close(clientfd);
    return;
  }

  google::protobuf::Service* service = it->second.m_service;
  const google::protobuf::MethodDescriptor* method = mit->second;

  // generate the request and response parameters of the RPC method call
  google::protobuf::Message* request =
      service->GetRequestPrototype(method).New();
  if (!request->ParseFromString(args_str)) {
    LOG(ERROR) << "request parse error, content: " << args_str;
    delete request;
    close(clientfd);
    return;
  }
  google::protobuf::Message* response =
      service->GetResponsePrototype(method).New();

  // set callback
  google::protobuf::Closure* done =
      google::protobuf::NewCallback<void()>([clientfd, request, response]() {
        std::string response_str;
        if (response->SerializeToString(&response_str)) {
          if (send(clientfd, response_str.c_str(), response_str.size(), 0) <
              0) {
            LOG(ERROR) << "send response error!";
          }
        } else {
          LOG(ERROR) << "serialize response error!";
        }
        delete request;
        delete response;
      });
  service->CallMethod(method, nullptr, request, response, done);
}