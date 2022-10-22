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

    // For now only used for syntax highlighting:
    int positionStart;
    int positionEnd;
    int positionStartUtf8Codepoint;
    int positionEndUtf8Codepoint;
};
