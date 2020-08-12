#pragma once

#include <string>
#include <vector>
#include <optional>

#include "Token.h"

class Tokenizer {
public:
    explicit Tokenizer(const std::string& input)
        : m_input(input)
    { }

    struct Options {
        Options() {}

        /* which character to count until `until` is satisfied
         * for example: for a subtokenizer tokenizing the inside of `$( ... )`,
         * Options#until should equal ')' and Options#countToUntil should be '('.
         * so that $( (cmd; (cmd)) ) is tokenized properly
         */
        std::optional<char> countToUntil; // TODO: counting parens is incorrect because the case statement
        std::optional<char> until;        //       has unmatched parens. This will work fine until case is implemented
                                          //       It also breaks in the case of `x=$(echo ${a/)/})`

        /* should the tokenizer handle comments? it's useful to have this option when
         * subtokenizing ${}, when # means string length and not a comment
         */
        bool handleComments = true;

        /* in subtokenizing we don't care about the tokenized output (it's swallowed as one
         * whole string), so make it possible to disable delimiting then to conserve time
         */
        bool delimit = true;
    };

    std::vector<Token> tokenize(const Options &opt = Options());

private:
    std::string m_input;
    size_t m_input_i = 0;
};
