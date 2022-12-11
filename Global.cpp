#include "Global.h"
#include <unistd.h>
#include <string>
#include "utils.h"
#include <filesystem>

Global g;

static std::optional<std::string> get_pwd() {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::current_path(ec);
    if(!ec) {
        return { path.string() };
    }
    return {};
}

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

    if(name == "PWD") {
        if(std::optional<std::string> pwd = get_pwd()) {
            return { *pwd };
        }
    }

    if(name == "PROMPT_PWD") {
        if(std::optional<std::string> pwd = get_pwd()) {
            if(char *home_cstr = getenv("HOME")) {
                std::string_view home(home_cstr);

                // normalize the path - remove last '/' characters
                while(home.size() > 0 && home.ends_with('/')) {
                    home.remove_suffix(1);
                }

                if(pwd->starts_with(home)) {
                    return { "~" + pwd->substr(home.size()) };
                } else {
                    return { *pwd };
                }
            }
        }
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

TemporaryVariableChange::TemporaryVariableChange(const std::string &name, const std::string &new_value)
    : name(name)
{
    auto oldVar = g.variables.find(name);
    if(oldVar == g.variables.end()) {
        was_set = false;
    } else {
        was_set = true;
        old_value = oldVar->second;
    }
    g.variables[name] = new_value;
}

TemporaryVariableChange::~TemporaryVariableChange() {
    if(was_set) {
        g.variables[name] = old_value;
    } else {
        g.variables.erase(name);
    }
}
