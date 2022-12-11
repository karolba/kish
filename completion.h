#pragma once
#include <vector>
#include <string>
#include "replxx.hxx"

namespace completion {

std::vector<replxx::Replxx::Completion> completion_callback(replxx::Replxx &replxx, std::string const &input, int &contextLen);

}

