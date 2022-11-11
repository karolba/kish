#pragma once
#include <string>
#include <vector>
#include "Token.h"

namespace executor {

void subshell_capture_output(const std::vector<Token> &tokens, std::string &out);
void run_from_string(const std::string &str);

}
