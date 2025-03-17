#ifndef _Papplication_HPP
#define _Papplication_HPP

#include <mutex>

#include "channel.hpp"
#include "conf.hpp"
#include "controller.hpp"

class Papplication {
 public:
  static void Init(int argc, char** argv);
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