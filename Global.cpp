#include "Global.h"
#include <unistd.h>
#include <string>

Global g;

std::optional<std::string> Global::get_variable(const std::string &name)
{
    if(name == "?") {
        return std::to_string(g.last_return_value);
    }

    auto search = g.variables.find(name);
    if(search != g.variables.end()) {
        return { search->second };
    }

    char *from_env = getenv(name.c_str());
    if(from_env)
        return { from_env };

    return {};
}
