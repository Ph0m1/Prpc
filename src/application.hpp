#ifndef _Papplication_HPP
#define _Papplication_HPP

#include "conf.hpp"
#include "channel.hpp"
#include "controller.hpp"
#include <mutex>

class Papplication{
public:
  static void Init(int argc, char **argv);
  static Papplication& GetInstance();
  static void deleteInstance();
  static Pconfig& GetConfig();
private:
  static Pconfig m_config;
  static Papplication* m_application; // Only
  static std::mutex m_mutex;
  Papplication(){}
  ~Papplication(){}
  Papplication(const Papplication&) = delete;
  Papplication(Papplication&&) = delete;
};

#endif