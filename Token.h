#pragma once

#include <string>

// POSIX: "The shell breaks the input into tokens: words and operators"
struct Token {
    enum class Type { 
        WORD,
        OPERATOR
    };

    Type type;
    std::string value;
};
