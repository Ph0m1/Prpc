#include "error.h"

namespace prpc {

// 定义静态成员变量
std::function<void(const PrpcException&)> ErrorHandler::global_error_handler_ = nullptr;

} // namespace prpc 