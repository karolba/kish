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
    get_simple_command().variable_assignments.push_back({name, value});
}

void Parser::commit_argument(const std::string &word)
{
    get_simple_command().argv.push_back(word);
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

    const Token *next = input_next_token();
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
        if (get_simple_command().variable_assignments.empty()
                && get_simple_command().argv.empty()
                && m_command.redirections.empty())
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
    if(!get_compound_command().command_list.empty()) {
        // TODO: Could this ever happen?
        throw SyntaxError{"Multiple command lists in one command?"};
    }

    read_command_list_into_until(get_compound_command().command_list, {"}"});
}

// This function gets called
//      if [HERE] conditions; then ...
// reads everything until 'fi', including the 'fi'
void Parser::read_commit_if()
{
    const Token *end_token;

    read_command_list_into_until(get_if_command().condition, {"then"});

    end_token = read_command_list_into_until(get_if_command().then, {"elif", "else", "fi"});
    while(end_token->value == "elif") {
        Command::If::Elif elif;
        read_command_list_into_until(elif.condition, {"then"});
        end_token = read_command_list_into_until(elif.then, {"elif", "else", "fi"});
        get_if_command().elif.emplace_back(std::move(elif));
    }
    if(end_token->value == "else") {
        get_if_command().opt_else.emplace();
        read_command_list_into_until(get_if_command().opt_else.value(), {"fi"});
    }
}

// This function gets called
//    while [HERE] conditions; do ..
// reads everything until 'done', including the 'done'
void Parser::read_commit_while() {
    read_command_list_into_until(get_while_command().condition, {"do"});
    read_command_list_into_until(get_while_command().body, {"done"});
}

const Token * Parser::read_command_list_into_until(CommandList& into, const std::vector<std::string_view> &until_commands)
{
    auto sub_tokens = m_input.subspan(m_input_i);

    size_t displacement;
    const Token *end_token;
    std::tie(into, displacement, end_token) = Parser(sub_tokens).parse_until(until_commands);
    m_input_i += displacement;

    return end_token;
}

const Token * Parser::input_next_token()
{
    if (m_input_i >= m_input.size())
        return nullptr;

    return &m_input[m_input_i++];
}

Command::Simple &Parser::get_simple_command()
{
    if(has_empty_command()) {
        m_command.value = Command::Simple{};
    }

    if(! std::holds_alternative<Command::Simple>(m_command.value)) {
        if(std::holds_alternative<Command::Compound>(m_command.value))
            throw SyntaxError{"{}-lists cannot take arguments"};

        if(std::holds_alternative<Command::If>(m_command.value))
            throw SyntaxError{"fi cannot take arguments"};

        throw SyntaxError{"Extra word"};
    }

    return std::get<Command::Simple>(m_command.value);
}

Command::Compound &Parser::get_compound_command()
{
    if(has_empty_command()) {
        m_command.value = Command::Compound{};
    }

    if(! std::holds_alternative<Command::Compound>(m_command.value)) {
        throw SyntaxError{"Compound commands cannot have environment variables passed to them"};
    }

    return std::get<Command::Compound>(m_command.value);
}

Command::If &Parser::get_if_command()
{
    if(has_empty_command()) {
        m_command.value = Command::If{};
    }

    if(! std::holds_alternative<Command::If>(m_command.value)) {
        if(std::holds_alternative<Command::Compound>(m_command.value))
            throw SyntaxError{"Missing ';' between '}' and 'if'"};

        if(std::holds_alternative<Command::Simple>(m_command.value))
            throw SyntaxError{"If statements cannot have environment variables passed to them"};

        throw SyntaxError{"Unexpected if"};
    }

    return std::get<Command::If>(m_command.value);
}

Command::While &Parser::get_while_command()
{
    if(has_empty_command()) {
        m_command.value = Command::While{};
    }

    if(! std::holds_alternative<Command::While>(m_command.value)) {
        if(std::holds_alternative<Command::Compound>(m_command.value))
            throw SyntaxError{"Missing ';' between '}' and 'while'"};

        if(std::holds_alternative<Command::Simple>(m_command.value))
            throw SyntaxError{"While loops cannot have environment variables passed to them"};

        throw SyntaxError{"Unexpected 'while'"};
    }

    return std::get<Command::While>(m_command.value);
}

bool Parser::has_empty_command()
{
    return std::holds_alternative<Command::Empty>(m_command.value);
}

void Parser::parse_token(const Token *token) {
    if (is_token_assignment(*token)) {
        commit_assignment(token->value);
    }
    else if (has_empty_command() && token->type == Token::Type::WORD && token->value == "{") {
        read_commit_compound_command_list();
    }
    else if (has_empty_command() && token->type == Token::Type::WORD && token->value == "if") {
        read_commit_if();
    }
    else if (has_empty_command() && token->type == Token::Type::WORD && token->value == "while") {
        read_commit_while();
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
    while (const Token * token = input_next_token()) {
        parse_token(token);
    }

    commit_command();
    commit_pipeline();
    commit_and_or_list();

    return std::move(m_command_list);
}

// Similar to Parser#parse(), but ends when it sees one of the commands specified in the parameter `commands`.
// Useful for recursively parsing language structures. For example, after a `while`, should always be a `do`
//
// Returns:
//  - A parsed command list
//  - how many tokens did it take to construct that command list
//  - a command from the parameter `commands` which caused the parsing to end
std::tuple<CommandList, size_t, const Token *> Parser::parse_until(const std::vector<std::string_view> &commands)
{
    while(const Token *token = input_next_token()) {
        if(has_empty_command() && token->type == Token::Type::WORD
                && std::find(commands.cbegin(), commands.cend(), token->value) != commands.cend())
        {
            commit_command();
            commit_pipeline();
            commit_and_or_list();

            return { std::move(m_command_list), m_input_i, token };
        }
        parse_token(token);
    }

    // TODO: this should prompt for more input
    if (commands.size() == 1) {
        throw SyntaxError{"'" + std::string(commands.at(0)) + "' expected, but got to the end of input"};
    } else {
        std::string err{"Either one of "};
        bool first = true;
        for(const auto &command : commands) {
            if(!first)
                err += ", ";
            err += '\'';
            err += command;
            err += '\'';

            first = false;
        }
        err += " expected, but got to the end of input";
        throw SyntaxError{err};
    }
}
