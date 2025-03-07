#include "controller.hpp"

Pcontroller::Pcontroller(){
  m_failed = false;
  m_errText = "";
}

void Pcontroller::Reset(){
  m_failed = false;
  m_errText = "";
}

bool Pcontroller::Failed() const{
  return m_failed;
}

std::string Pcontroller::ErrorText() const {
  return m_errText;
}

void Pcontroller::SetFailed(const std::string &reason){
  m_failed = true;
  m_errText = reason;
}

// disabled
void Pcontroller::StartCancel(){}
bool Pcontroller::IsCanceled() const {
    return false;
}
void Pcontroller::NotifyOnCancel(google::protobuf::Closure* callback) {}