#pragma once 

#include <sstream>
#include <fstream>
#include <cstdlib>

inline auto read_file_to_string(std::string filepath)
{
    std::ifstream t(filepath);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

inline auto get_address(std::string symbol_map_text, std::string target_variable)
{
    auto address_index = symbol_map_text.rfind("0x0", symbol_map_text.find(target_variable));
    return strtoul(symbol_map_text.c_str() + address_index, nullptr, 16);
}