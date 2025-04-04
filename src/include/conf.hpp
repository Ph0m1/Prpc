#ifndef _Pconfig_HPP
#define _Pconfig_HPP
#include <unordered_map>
#include <string>
#include "error.hpp"

class Pconfig{
    public:
    // 使用Result返回类型，提供更好的错误处理
    prpc::Result<void> LoadConfigFile(const char *config_file);
    std::string Load(const std::string &key);
    private:
    std::unordered_map<std::string, std::string> config_map;
    void Trim(std::string &read_buf);

};

#endif