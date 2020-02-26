#pragma once

#include <string>
#include <vector>

#include "Token.h"

class Tokenizer {
public:
    explicit Tokenizer(const std::string& input)
        : m_input(input)
    { }

    std::vector<Token> tokenize();

private:
    std::string m_input;
};
