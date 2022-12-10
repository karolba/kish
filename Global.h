#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "Parser.h"

struct Global {
    std::unordered_map<std::string, CommandList> functions;

    std::unordered_map<std::string, std::string> variables;
    int last_return_value = 0; // "$?"

    std::vector<std::unordered_map<std::string, std::string>> scoped_variables;

    // The "$@". Temporarily replaced with different values when in a function
    std::vector<std::string> argv;

    std::optional<std::string> get_variable(const std::string &name);

    struct SavedFd {
        int original_fd;
        int saved_location;
    };
    std::vector<SavedFd> saved_fds;
};

class TemporaryVariableChange {
public:
    TemporaryVariableChange(const std::string &name, const std::string &new_value);
    ~TemporaryVariableChange();

private:
    bool was_set;
    const std::string name;
    std::string old_value;
};


extern Global g;

//void set_variable(const std::string &name, const std::string &value);

