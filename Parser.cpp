#include "Parser.h"
#include "Token.h"
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <utility>
#include <string>

static bool is_token_assignment(const Token &token)
{
    if (token.type != Token::Type::WORD)
        return false;

    for (size_t i = 0; i < token.value.length(); ++i) {
        if (i > 0 && token.value[i] == '=')
            return true;
        if (!isalnum(token.value[i]) && token.value[i] != '_') {
            return false;
        }
    }

    return false;
}

void Parser::commit_assignment(const std::string &assignment)
{
    size_t equals_pos = assignment.find('=');
    std::string name = assignment.substr(0, equals_pos);
    std::string value = assignment.substr(equals_pos + 1);
    m_command.variable_assignments.push_back({name, value});
}

void Parser::commit_argument(const std::string &word)
{
    m_command.argv.push_back(word);
}

void Parser::commit_redirection(const std::string &op)
{
    Redirection::Type type = Redirection::FileWrite;
    int fd { 1 };
    if (op == ">") { 
        type = Redirection::FileWrite;
    } else if (op == ">>") {
        type = Redirection::FileWriteAppend;
    } else if (op == "<") {
        type = Redirection::FileRead;
        fd = 0;
    }

    Token *next = input_next_token();
    if (next == nullptr) {
        fprintf(stderr, "Syntax error: EOF after a redirection operator\n");
        exit(1);
    }
    if (next->type == Token::Type::OPERATOR) {
        fprintf(stderr, "Syntax error: operator or newline after a redirection operator (expected a word)\n");
        exit(1);
    }
    
    m_command.redirections.push_back({type, fd, -1, move(next->value)});
}

void Parser::commit_command()
{
    if (m_command.variable_assignments.empty()
            && m_command.argv.empty()
            && m_command.redirections.empty())
        return;

    m_pipeline.push_back(m_command); // TODO: std::move(m_command) ??
    m_command = {};
}

void Parser::commit_pipeline(const Token* op)
{
    if (m_pipeline.empty())
        return;

    m_and_or_list.push_back({move(m_pipeline), op ? op->value : ""});

    m_pipeline = {};
}

void Parser::commit_and_or_list(const Token* op)
{
    if (m_and_or_list.empty())
        return;

    m_command_list.push_back({move(m_and_or_list), op ? op->value : ""});

    m_and_or_list = {};
}

Token* Parser::input_next_token()
{
    if (m_input_i >= m_input.size())
        return nullptr;

    return &m_input[m_input_i++];
}

CommandList Parser::parse()
{
    Token *token;
    while ((token = input_next_token())) {
        if (is_token_assignment(*token)) {
            commit_assignment(token->value);
        }
        else if (token->type == Token::Type::WORD) {
            commit_argument(token->value);
        }
        // Operators:
        else if (token->value == ">" || token->value == "<" || token->value == ">>") {
            commit_redirection(token->value);
        }
        else if (token->value == "|") {
            commit_command();
        }
        else if (token->value == "&&" || token->value == "||") {
            commit_command();
            commit_pipeline(token);
        }
        else if (token->value == ";" || token->value == "&" || token->value == "\n") {
            commit_command();
            commit_pipeline();
            commit_and_or_list(token);
        }
    }

    commit_command();
    commit_pipeline();
    commit_and_or_list();

    return move(m_command_list);
}
