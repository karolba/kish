#include <string.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "Tokenizer.h"
#include "Token.h"
#include "utils.h"

static bool can_start_operator(char c) {
    return strchr("&<>;|()\n", c) != nullptr;
}

static bool can_extend_operator(char c1, char c2) {
    return
        (c1 == '&' && c2 == '&') ||
        (c1 == '|' && c2 == '|') ||
        (c1 == ';' && c2 == ';') ||
        (c1 == '<' && c2 == '<') ||
        (c1 == '>' && c2 == '>') ||
        (c1 == '<' && c2 == '&') ||
        (c1 == '>' && c2 == '&') ||
        (c1 == '<' && c2 == '>') ||
        (c1 == '(' && c2 == ')');
}


void Tokenizer::delimit(std::vector<Token> &output, std::string &current_token, Token::Type token_type, int position) {
    if (current_token.length() == 0) {
        return;
    }
    int start = position - current_token.size();
    int end = position;

    // TODO: this could propably be quicker than computing the whole utf-8 length every time
    int untilTokenCodepointLen = utils::utf8_codepoint_len(input, start);
    int tokenCodepointLen = utils::utf8_codepoint_len(current_token.c_str(), current_token.size());

    int utf8CodepointStart = untilTokenCodepointLen;
    int utf8CodepointEnd = untilTokenCodepointLen + tokenCodepointLen;

    output.push_back(Token {
                         token_type,
                         current_token,
                         start,
                         end,
                         utf8CodepointStart,
                         utf8CodepointEnd
                     });
    current_token.clear();
}

std::vector<Token> Tokenizer::tokenize(const Tokenizer::Options &opt) {
    std::vector<Token> output;

    bool quoted_single = false;
    bool quoted_double = false;
    bool in_operator = false;

    std::string current_token;

    // Count `(`s in `$( (cmd) )`
    int openParensCount = 0;

    for(; input_i < input.length(); input_i++) {
        char ch = input[input_i];

        // IEEE Std 1003.1-2017 Shell Command Language 2.3.2
        if (in_operator && !quoted_single && !quoted_double && can_extend_operator(input[input_i - 1], ch)) {
            current_token.push_back(ch);
            continue;
        }

        // 2.3.3
        if (in_operator && !can_extend_operator(input[input_i - 1], ch)) {
            if(opt.delimit)
                delimit(output, current_token, Token::Type::OPERATOR, input_i);

            in_operator = false;
            // intentially no contiune
        }

        // 2.3.4
        if (!quoted_single && ch == '\\') {
            current_token.push_back(ch);

            if (input_i + 1 == input.length()) {
                throw SyntaxError{"Tokenizer error: Nothing after a backslash"};
            }
            // TODO: newline joining and continue if can get more input
            input_i += 1;
            current_token.push_back(input[input_i]);
            continue;
        }

        if (!quoted_single && !quoted_double && ch == '"') {
            current_token.push_back(ch);
            quoted_double = true;
            continue;
        }

        if (!quoted_single && !quoted_double && ch == '\'') {
            current_token.push_back(ch);
            quoted_single = true;
            continue;
        }

        if (quoted_single && ch != '\'') {
            current_token.push_back(ch);
            continue;
        }

        if (quoted_single && ch == '\'') {
            quoted_single = false;
            current_token.push_back(ch);
            continue;
        }

        // Shell Command Language 2.3.5
        // Recursively tokenize command substitiution ($() and ``), arithmetic expansion ($(()) and $[])
        // and parameter expansion (${}, can have whitespace in them sometimes) 
        if(!quoted_single && ch == '$' && input_i + 1 < input.length()) {
            char next_ch = input[input_i + 1];

            size_t index_before_subtokenization = input_i;

            if(next_ch == '(') { // `$(`
                Options sub_opt;
                sub_opt.delimit = false;
                sub_opt.countToUntil = '(';
                sub_opt.until = ')';
                sub_opt.handleComments = true;

                input_i += strlen("$(");
                tokenize(sub_opt);

            } else if(next_ch == '{') { // `${`
                // TODO: is this correct? ${}-expansion is treated here like command substitution (minus '#'),
                // yet it isn't in the first name part. Can't think of a case where this would fail though.
                Options sub_opt;
                sub_opt.delimit = false;
                sub_opt.until = '}';
                sub_opt.handleComments = false; // don't treat '#' as comments (`${#var}`)

                input_i += strlen("${");
                tokenize(sub_opt);
            }
            //

            size_t subtokenized_len = input_i - index_before_subtokenization + 1; // `+ 1` because of ')' or '}'
            current_token.append(input.substr(index_before_subtokenization, subtokenized_len));
            continue;
        }
        // TODO: ``


        if (quoted_double && ch != '\"') {
            current_token.push_back(ch);
            continue;
        }

        if (quoted_double && ch == '\"') {
            quoted_double = false;
            current_token.push_back(ch);
            continue;
        }

        // recursive tokenizing - count '(' in $( (cmd1); (cmd2;(cmd3)) )
        if(!quoted_double && !quoted_single && opt.countToUntil.has_value() && ch == opt.countToUntil.value()) {
            openParensCount += 1;
            // intentionally no continue - the token should end up in the output
        }

        // recursive tokenizing - count ')' in $( (cmd1); (cmd2;(cmd3)) )
        if(!quoted_double && !quoted_single && opt.until.has_value() && ch == opt.until.value()) {
            if(openParensCount == 0) {
                if(opt.delimit)
                    delimit(output, current_token, Token::Type::WORD, input_i);

                return output;
            }

            openParensCount -= 1;
            // intentionally no continue - the token should end up in the output
        }


        // 2.3.6
        if (!quoted_single && !quoted_double && can_start_operator(ch)) {
            if(opt.delimit)
                delimit(output, current_token, Token::Type::WORD, input_i);
            in_operator = true;
            current_token.push_back(ch);
            continue;
        }

        // 2.3.7
        if (strchr(" \t\r", ch) != nullptr) {
            if(opt.delimit)
                delimit(output, current_token, Token::Type::WORD, input_i);
            continue;
        }

        // 2.3.8
        if (current_token.length() != 0) {
            current_token.push_back(ch);
            continue;
        }

        // 2.3.9
        if (opt.handleComments && ch == '#') {
            while (input_i < input.length() && input[input_i] != '\n')
                ++input_i;
            continue;
        }

        // 2.3.10
        if (current_token.length() == 0) {
            current_token.push_back(ch);
            continue;
        }
    }

    // 2.3.1
    if(opt.delimit) {
        if (in_operator) {
            delimit(output, current_token, Token::Type::OPERATOR, input_i);
        } else {
            delimit(output, current_token, Token::Type::WORD, input_i);
        }
    }

    if(opt.until.has_value()) {
        // TODO: this should ask for more input
        throw SyntaxError{"Tokenizer error: no end of \"$(\"/\"${\""};
    }

    return output;
}

size_t Tokenizer::consumedChars()
{
    return input_i;
}
