#include <string.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "Tokenizer.h"
#include "Token.h"

static bool can_start_operator(char c) {
    return strchr("&<>;|\n", c) != nullptr;
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
        (c1 == '<' && c2 == '>');
}

static void delimit(std::vector<Token> &output, std::string &current_token, Token::Type token_type) {
    if (current_token.length() == 0) {
        return;
    }
    output.push_back(Token { token_type, current_token });
    current_token.clear();
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> output;

    bool quoted_single = false;
    bool quoted_double = false;
    bool in_operator = false;

    std::string current_token;

    for(size_t i = 0; i < m_input.length(); i++) {
        char ch = m_input[i];

        // IEEE Std 1003.1-2017 Shell Command Language 2.3.2
        if (in_operator && !quoted_single && !quoted_double && can_extend_operator(m_input[i - 1], ch)) {
            current_token.push_back(ch);
            continue;
        }

        // 2.3.3
        if (in_operator && !can_extend_operator(m_input[i - 1], ch)) {
            delimit(output, current_token, Token::Type::OPERATOR);
            in_operator = false;
            // intentially no contiune
        }

        // 2.3.4
        if (!quoted_single && ch == '\\') {
            current_token.push_back(ch);

            if (i + 1 == m_input.length()) {
                fprintf(stderr, "Syntax error: Nothing after a backslash\n");
                exit(1);
            }
            // TODO: newline joining and continue if can get more input
            i += 1;
            current_token.push_back(m_input[i]);
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

        // TODO: Shell Command Language 2.3.5 
        // Tokenize command substitiution ($() and ``), arithmetic expansion ($(()) and $[])
        // and parameter expansion (${}, can have whitespace in them sometimes) 
        
        if (quoted_double && ch != '\"') {
            current_token.push_back(ch);
            continue;
        }

        if (quoted_double && ch == '\"') {
            quoted_double = false;
            current_token.push_back(ch);
            continue;
        }

        // 2.3.6
        if (!quoted_single && !quoted_double && can_start_operator(ch)) {
            delimit(output, current_token, Token::Type::WORD);
            in_operator = true;
            current_token.push_back(ch);
            continue;
        }

        // 2.3.7
        if (strchr(" \t\r", ch) != nullptr) {
            delimit(output, current_token, Token::Type::WORD);
            continue;
        }

        // 2.3.8
        if (current_token.length() != 0) {
            current_token.push_back(ch);
            continue;
        }

        // 2.3.9
        if (ch == '#') {
            while (i < m_input.length() && m_input[i] != '\n') 
                ++i;
            continue;
        }

        // 2.3.10
        if (current_token.length() == 0) {
            current_token.push_back(ch);
            continue;
        }
    }

    // 2.3.1
    if (in_operator)
        delimit(output, current_token, Token::Type::OPERATOR);
    else
        delimit(output, current_token, Token::Type::WORD);

    return output;
}
