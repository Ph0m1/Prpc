#include "application.hpp"

#include <unistd.h>

#include <cstdlib>
#include <iostream>

Pconfig Papplication::m_config;
std::mutex Papplication::m_mutex;
Papplication* Papplication::m_application = nullptr;

void Papplication::Init(int argc, char** argv) {
  if (argc < 2) {  // none config progame
    std::cout << "Format should use: command -i <config_file_path>"
              << std::endl;
  }

  int o;
  std::string config_file;
  while (-1 != (o = getopt(argc, argv, "i:"))) {
    switch (o) {
      case 'i':
        config_file = optarg;
        break;
      case '?':
      case ':':
        std::cout << "Format should use: command -i <config_file_path>"
                  << std::endl;
        exit(EXIT_FAILURE);
        break;
      default:
        break;
    }
  }
  m_config.LoadConfigFile(config_file.c_str());
}

Papplication &Papplication::GetInstance() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_application == nullptr){
    m_application = new Papplication();
    atexit(deleteInstance);
  }
  return *m_application;
}

// Destory sample
void Papplication::deleteInstance() {
  if (m_application) {
    delete m_application;
  }
}

Pconfig& Papplication::GetConfig() {
  return m_config;
}