#pragma once

#include <string>
#include <vector>

#include "Parser.h"

class CommandExpander {
public:
    explicit CommandExpander(Command* command) 
        : m_command(command)
    { }

    bool expand();

private:
    Command* m_command;
    void expand_word(const std::string& word, std::vector<std::string> &out);
    void word_expansion_failed(const std::string &word);
};
