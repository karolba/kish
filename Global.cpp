#include "Global.h"
#include <unistd.h>
#include <string>
#include "utils.h"

Global g;

// TODO: return std::optional<std::string_view> ?
std::optional<std::string> Global::get_variable(const std::string &name)
{
    if(name == "?") {
        return std::to_string(g.last_return_value);
    }

    // $0, $1, ${123}, ...
    if(name.length() >= 1 && utils::no_locale_isdigit(name.at(0))) {
        size_t arg_i = atoll(name.c_str());
        if(arg_i >= argv.size())
            return { "" };
        return { argv.at(arg_i) };
    }

    if(name == "*") {
        std::string all_args {};
        for(size_t i = 1; i < g.argv.size(); i++) {
            if(i != 1)
                all_args.push_back(' ');
            all_args.append(g.argv.at(i));
        }
        return { all_args };
    }

    // TODO: write a test for this
    if(name == "#") {
        return std::to_string(g.argv.size() - 1);
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
