#include "application.h"
#include "error.h"

#include <unistd.h>

#include <cstdlib>
#include <iostream>

Pconfig Papplication::m_config;
std::mutex Papplication::m_mutex;
Papplication* Papplication::m_application = nullptr;

prpc::Result<void> Papplication::Init(int argc, char** argv) {
  try {
    if (argc < 2) {  // none config progame
      throw prpc::ConfigException("Format should use: command -i <config_file_path>");
    }

    // 重置getopt状态，支持多次调用
    optind = 1;
    
    int o;
    std::string config_file;
    while (-1 != (o = getopt(argc, argv, "i:"))) {
      switch (o) {
        case 'i':
          config_file = optarg;
          break;
        case '?':
        case ':':
          throw prpc::ConfigException("Format should use: command -i <config_file_path>");
          break;
        default:
          break;
      }
    }
    
    if (config_file.empty()) {
      throw prpc::ConfigException("No configuration file specified");
    }
    
    auto result = m_config.LoadConfigFile(config_file.c_str());
    if (!result.isSuccess()) {
      throw prpc::ConfigException("Failed to load config file: " + result.getErrorMessage());
    }
    
    return prpc::Result<void>();
  } catch (const prpc::PrpcException& e) {
    return prpc::Result<void>(e.getErrorCode(), e.what());
  } catch (const std::exception& e) {
    return prpc::Result<void>(prpc::ErrorCode::UNKNOWN_ERROR, e.what());
  }
}

Papplication& Papplication::GetInstance() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_application == nullptr) {
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

Pconfig& Papplication::GetConfig() { return m_config; }