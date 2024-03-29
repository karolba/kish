#pragma once

#include <string>
#include <vector>
#include <optional>

#include "Token.h"

class Tokenizer {
public:
    explicit Tokenizer(const std::string_view input)
        : input(input)
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

    struct SyntaxError {
        SyntaxError(const std::string &explanation)
            : explanation(explanation)
        {}
        std::string explanation;
    };

    std::vector<Token> tokenize(const Options &opt = Options());
    size_t consumedChars();

    Tokenizer &dontThrowOnIncompleteInput() { throwOnIncompleteInput = false; return *this; }

private:
    std::string_view input;
    size_t input_i = 0;

    // set to none when tokenizing input on <tab> presses
    bool throwOnIncompleteInput = true;

    void delimit(std::vector<Token> &output, std::string &current_token, Token::Type token_type, int position);
};
