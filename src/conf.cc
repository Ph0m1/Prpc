#include "conf.hpp"

#include <memory>

void Pconfig::LoadConfigFile(const char* config_file){
    std::unique_ptr<FILE, decltype(&fclose)>pf(
        fopen(config_file, "r"),
        &fclose
    );

    if (pf == nullptr){ exit(EXIT_FAILURE); }

    char buf[1024];
    while(fgets(buf, 1024, pf.get()) != nullptr) {
        std::string read_buf(buf);
        Trim(read_buf); // 移除空格

        if(read_buf[0] == '#' || read_buf.empty()) continue;

        int index = read_buf.find('=');
        if (index == -1) continue;

        // 提取key
        std::string key = read_buf.substr(0, index);
        Trim(key);

        int endindex = read_buf.find('\n', index);
        //提取 value
        std::string value = read_buf.substr(index+1, endindex-index-1);
        Trim(value);

        config_map.insert({key, value});
    }
}

// search by key
std::string Pconfig::Load(const std::string &key) {
    auto it = config_map.find(key);
    if(it == config_map.end()){
        return "";
    }
    return it->second;
}

void Pconfig::Trim(std::string &buf) {
    int index = buf.find_first_not_of(' ');
    if (index != -1) {
        buf = buf.substr(index, buf.size() - index);
    }
    
    index = buf.find_last_not_of(' ');
    if (index != -1) {
        buf = buf.substr(0, index + 1);
    }
}