#include "../user.pb.h"
#include "application.h"
#include "provider.h"

class UserService : public Puser::UserServiceRpc {
 public:
  // simulate login logic
  bool Login(std::string name, std::string pwd) {
    std::cout << "doing local service: Login" << std::endl;
    std::cout << "name: " << name << " pwd: " << pwd << std::endl;
    return true;
  }

  void Login(::google::protobuf::RpcController* controller,
             const ::Puser::LoginRequest* request,
             ::Puser::LoginResponse* response,
             ::google::protobuf::Closure* done) {
    // get username an passwd
    std::string name = request->name();
    std::string pwd = request->pwd();

    // login
    bool login_result = Login(name, pwd);

    // write result to response
    Puser::ResultCode* code = response->mutable_result();
    code->set_errcode(0);                 // err = 0, success
    code->set_errmsg("");                 // errinfo = null
    response->set_success(login_result);  // set login

    done->Run();
  }
};

int main(int argc, char** argv) {
  // init
  Papplication::Init(argc, argv);
  Pprovider provider;

  // notify UserService to RPC node
  provider.NotifyService(new UserService());

  // run RPC node, waitting for RPC
  provider.Run();
  return 0;
}