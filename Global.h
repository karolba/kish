#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct Global {
    std::unordered_map<std::string, std::string> variables;
    int last_return_value = 0; // "$?"

    std::optional<std::string> get_variable(const std::string &name);
};

extern Global g;

//void set_variable(const std::string &name, const std::string &value);

