#include "builtins.h"

#include "builtins/false.h"
#include "builtins/true.h"
#include "builtins/help.h"
#include "builtins/cd.h"
#include "builtins/colon.h"
#include "builtins/read.h"
#include "builtins/source.h"
#include "builtins/echo.h"

#include <map>
#include <unordered_map>

const std::unordered_map<std::string, BuiltinHandler> *get_builtins() {
    const static std::unordered_map<std::string, BuiltinHandler> builtins {
        {"cd", builtin_cd},
        {"false", builtin_false},
        {"true", builtin_true},
        {"help", builtin_help},
        {":", builtin_colon},
        {"read", builtin_read},
        {"source", builtin_source},
        {"echo", builtin_echo}
    };

    return &builtins;
}


std::optional<BuiltinHandler> find_builtin(const std::string &name) {
    auto foundBuiltin = get_builtins()->find(name);

    if(foundBuiltin != get_builtins()->end()) {
        return foundBuiltin->second;
    } else {
        return {};
    }
}
