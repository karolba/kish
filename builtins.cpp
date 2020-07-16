#include "builtins.h"

#include "builtins/false.h"
#include "builtins/true.h"
#include "builtins/help.h"
#include "builtins/cd.h"
#include "builtins/colon.h"
#include "builtins/read.h"

#include <map>

std::optional<BuiltinHandler> find_builtin(const std::string &name) {
    const static std::unordered_map<std::string, BuiltinHandler> builtins {
        {"cd", builtin_cd},
        {"false", builtin_false},
        {"true", builtin_true},
        {"help", builtin_help},
        {":", builtin_colon},
        {"read", builtin_read},
    };

    auto foundBuiltin = builtins.find(name);

    if(foundBuiltin != builtins.end()) {
        return foundBuiltin->second;
    } else {
        return {};
    }
}
