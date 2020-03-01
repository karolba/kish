#include "Parser.h"
#include "Token.h"
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <utility>
#include <string>
#include <variant>
#include <iostream>
#include <tuple>

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
    get_simple_command()->variable_assignments.push_back({name, value});
}

void Parser::commit_argument(const std::string &word)
{
    get_simple_command()->argv.push_back(word);
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
        throw SyntaxError{"End of input after a redirection operator"};
    }
    if (next->type == Token::Type::OPERATOR) {
        throw SyntaxError{"operator or newline after a redirection operator (expected a word)"};
    }
    
    m_command.redirections.push_back({type, fd, -1, std::move(next->value)});
}

void Parser::commit_command()
{
    if(std::holds_alternative<Command::Empty>(m_command.value)) {
        if(m_command.redirections.empty())
            return;
    }

    if(std::holds_alternative<Command::Simple>(m_command.value)) {
        if (get_simple_command()->variable_assignments.empty()
                && get_simple_command()->argv.empty()
                && m_command.redirections.empty())
            return;
    }

    if(std::holds_alternative<Command::Compound>(m_command.value)) {
        if(m_command.redirections.empty()
                && get_compound_command()->command_list.empty())
            return;
    }

    m_pipeline.push_back(std::move(m_command));
    m_command = {};
}

void Parser::commit_pipeline(const Token* op)
{
    if (m_pipeline.empty())
        return;

    m_and_or_list.push_back({std::move(m_pipeline), op ? op->value : ""});
    m_pipeline = {};
}

void Parser::commit_and_or_list(const Token* op)
{
    if (m_and_or_list.empty())
        return;

    m_command_list.push_back({std::move(m_and_or_list), op ? op->value : ""});
    m_and_or_list = {};
}

void Parser::read_commit_compound_command_list()
{
    if(!get_compound_command()->command_list.empty()) {
        // `{ command; } { command; }`
        throw SyntaxError{"Multiple command lists in one command are not allowed, did you forget a ';'?"};
    }

    auto i = static_cast<std::vector<Token>::difference_type>(m_input_i);
    std::vector<Token> sub_tokens = std::vector(m_input.cbegin() + i, m_input.cend());

    size_t displacement;
    std::tie(get_compound_command()->command_list, displacement) = Parser(sub_tokens).parse_compound_command_list();
    m_input_i += displacement; // TODO: - 1? + 1?
}

Token* Parser::input_next_token()
{
    if (m_input_i >= m_input.size())
        return nullptr;

    return &m_input[m_input_i++];
}

Command::Simple *Parser::get_simple_command()
{
    if(has_empty_command()) {
        m_command.value = Command::Simple{};
    }

    if(! std::holds_alternative<Command::Simple>(m_command.value)) {
        throw SyntaxError{"Compound commands cannot take arguments"};
    }

    return &std::get<Command::Simple>(m_command.value);
}

Command::Compound *Parser::get_compound_command()
{
    if(has_empty_command()) {
        m_command.value = Command::Compound{};
    }

    if(! std::holds_alternative<Command::Compound>(m_command.value)) {
        throw SyntaxError{"Compound commands cannot have environment variables passed to them"};
    }

    return &std::get<Command::Compound>(m_command.value);
}

bool Parser::has_empty_command()
{
    return std::holds_alternative<Command::Empty>(m_command.value);
}

void Parser::parse_token(Token *token) {
    if (is_token_assignment(*token)) {
        commit_assignment(token->value);
    }
    else if (has_empty_command() && token->type == Token::Type::WORD && token->value == "{") {
        read_commit_compound_command_list();
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

CommandList Parser::parse()
{
    Token *token;
    while ((token = input_next_token())) {
        parse_token(token);
    }

    commit_command();
    commit_pipeline();
    commit_and_or_list();

    return std::move(m_command_list);
}

std::pair<CommandList, size_t> Parser::parse_compound_command_list() {
    Token *token;
    while((token = input_next_token())) {
        if(has_empty_command() && token->type == Token::Type::WORD && token->value == "}") {
            commit_command();
            commit_pipeline();
            commit_and_or_list();

            return { std::move(m_command_list), m_input_i };
        }
        parse_token(token);
    }

    // TODO: this should prompt for more input
    throw SyntaxError{"Compound command: '}' expected but got to the end of input"}; // No '}' on the current line
}
