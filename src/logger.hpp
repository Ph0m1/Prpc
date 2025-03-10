#ifndef P_LOG_HPP
#define P_LOG_HPP
#include <glog/logging.h>

#include <string>

class PLogger {
 public:
  explicit PLogger(const char* argv0) {
    google::InitGoogleLogging(argv0);
    FLAGS_colorlogtostderr = true;
    FLAGS_logtostderr = true;
  }
  ~PLogger() { google::ShutdownGoogleLogging(); }
  static void Info(const std::string& message) { LOG(INFO) << message; }
  static void Warning(const std::string& message) { LOG(WARNING) << message; }
  static void ERROR(const std::string& message) { LOG(ERROR) << message; }
  static void Fatal(const std::string& message) { LOG(FATAL) << message; }

 private:
  PLogger(const PLogger&) = delete;
  PLogger& operator=(const PLogger&) = delete;
};

#endif