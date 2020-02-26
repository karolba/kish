#include "Global.h"
#include <unistd.h>

Global g;

std::optional<std::string> Global::get_variable(const std::string &name)
{
    auto search = g.variables.find(name);
    if(search == g.variables.end()) {
        char *from_env = getenv(name.c_str());
        if(!from_env)
            return {};
        return { from_env };
    }
    return { search->second };
}
