#include "builtins.h"

#include "builtins/false.h"
#include "builtins/true.h"
#include "builtins/help.h"
#include "builtins/cd.h"
#include "builtins/colon.h"

std::optional<BuiltinHandler> find_builtin(const std::string &name) {
    if(name == "false")
        return builtin_false;

    if(name == "true")
        return builtin_true;

    if(name == "help")
        return builtin_help;

    if(name == "cd")
        return builtin_cd;

    if(name == ":")
        return builtin_colon;

    return {};
}
