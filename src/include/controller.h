#ifndef _Pcontroller_H
#define _Pcontroller_H

#include <google/protobuf/service.h>

#include <string>

class Pcontroller : public google::protobuf::RpcController {
 public:
  Pcontroller();
  void Reset() override;
  bool Failed() const override;
  std::string ErrorText() const override;
  void SetFailed(const std::string& reason) override;

  void StartCancel() override;
  bool IsCanceled() const override;
  void NotifyOnCancel(google::protobuf::Closure* callback) override;

  void SetTimeout(int timeout_ms);
  int GetTimeout() const;
 private:
  bool m_failed;
  std::string m_errText;
  int m_timeout_ms;
};

#endif