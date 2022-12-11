#pragma once
#include <string>
#include <vector>
#include "Token.h"
#include "Parser.h"

namespace executor {

void subshell_capture_output(const std::vector<Token> &tokens, std::string &out);
void subshell_capture_output(const CommandList &parsed, std::string &out);
void run_from_string(const std::string &str);

}
