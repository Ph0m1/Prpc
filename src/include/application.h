#ifndef _Papplication_H
#define _Papplication_H

#include <mutex>

#include "channel.h"
#include "conf.h"
#include "controller.h"

class Papplication {
 public:
  static prpc::Result<void> Init(int argc, char** argv);
  static Papplication& GetInstance();
  static void deleteInstance();
  static Pconfig& GetConfig();

 private:
  static Pconfig m_config;
  static Papplication* m_application;  // Only
  static std::mutex m_mutex;
  Papplication() {}
  ~Papplication() {}
  Papplication(const Papplication&) = delete;
  Papplication(Papplication&&) = delete;
};

#endif