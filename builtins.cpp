#include "builtins.h"

#include <optional>
#include <functional>
#include <string>
#include "Parser.h"

static int builtin_false(const Command::Simple &) {
    return 1;
}

static int builtin_true(const Command::Simple &) {
    return 0;
}

static int builtin_help(const Command::Simple &) {
    fprintf(stdout, "This is some help\n");
    return 0;
}

std::optional<BuiltinHandler> find_builtin(const std::string &name) {
    if(name == "false")
        return builtin_false;

    if(name == "true")
        return builtin_true;

    if(name == "help")
        return builtin_help;

    return {};
}
