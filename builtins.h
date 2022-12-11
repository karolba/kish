#pragma once

#include <optional>
#include <functional>
#include <string>
#include "Parser.h"

using BuiltinHandler = std::function<int(const Command::Simple &)>;

std::optional<BuiltinHandler> find_builtin(const std::string &name);
const std::unordered_map<std::string, BuiltinHandler> *get_builtins();
