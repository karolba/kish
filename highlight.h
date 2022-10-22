#pragma once

#include <string>
#include "replxx.hxx"

namespace highlight {

void highlighter_callback(std::string const &input, replxx::Replxx::colors_t &colors);

}
