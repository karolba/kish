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

void Parser::commit_argument(const std::string &word, const Token *token_for_highlighting)
{
    get_simple_command().argv.push_back(word);
    get_simple_command().argv_tokens.push_back(token_for_highlighting);
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
    if(command_is_type<Command::Empty>()) {
        if(m_command.redirections.empty())
            return;
    }

    if(command_is_type<Command::Simple>()) {
        if (get_simple_command().variable_assignments.empty()
                && get_simple_command().argv.empty()
                && m_command.redirections.empty())
            return;
    }

    m_command.end_token = &m_input[m_input_i - 1];
    m_pipeline.commands.push_back(std::move(m_command));
    m_command = {};
}

void Parser::commit_pipeline(const Token* op)
{
    if (m_pipeline.commands.empty() && m_pipeline.negation_prefix == false)
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
    if(!get_brace_group().command_list.empty()) {
        // TODO: Could this ever happen?
        throw SyntaxError{"Multiple command lists in one command?"};
    }

    read_command_list_into_until(get_brace_group().command_list, {"}"});
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

// This function gets called
//    until [HERE] conditions; do ..
// reads everything until 'done', including the 'done'
void Parser::read_commit_until() {
    read_command_list_into_until(get_until_command().condition, {"do"});
    read_command_list_into_until(get_until_command().body, {"done"});
}

void Parser::for_loop_add_item(const Token *token) {
    get_for_command().items.emplace_back(token->value);
    get_for_command().items_tokens.emplace_back(token);
}

// This function gets called
//    for [HERE] [in ...]; do ...
// reads everything until 'done', including the 'done'
void Parser::read_commit_for() {
    const Token *variable_name = input_next_token();
    if(variable_name == nullptr)
        throw SyntaxError{"End of input"};

    if(variable_name->type == Token::Type::OPERATOR) {
        throw SyntaxError{variable_name};
    }
    get_for_command().varname = variable_name->value;

    // token: `for x [in|do|;|\n]`
    const Token *after_varname = input_next_token();
    if(after_varname == nullptr)
        throw SyntaxError{"End of input"};

    if(after_varname->type == Token::Type::OPERATOR && (after_varname->value == ";" || after_varname->value == "\n")) {
        // The `for var; do ...` form

        get_for_command().items = {"\"$@\""}; // `for var; do`  ==  `for var in "$@"; do`
    } else if(after_varname->type == Token::Type::WORD && after_varname->value == "in") {
        // The `for var in WORD...; do ...` form

        while((after_varname = input_next_token())) {
            if(after_varname->type == Token::Type::OPERATOR && (after_varname->value == ";" || after_varname->value == "\n"))
                break;

            if(after_varname->type == Token::Type::OPERATOR)
                throw SyntaxError{after_varname};

            for_loop_add_item(after_varname);
        }

        if(after_varname == nullptr)
            throw SyntaxError{"End of input"};
    } else if(after_varname->type == Token::Type::WORD && after_varname->value == "do") {
        // The `for var do ...` form

        input_put_token_back(); // put the `do` back
    } else {
        throw SyntaxError{after_varname};
    }

    const Token *compound_start = input_next_token();

    if(compound_start == nullptr)
        throw SyntaxError{"End of input"};

    while(compound_start->type == Token::Type::OPERATOR && compound_start->value == "\n") {
        compound_start = input_next_token();
        if(compound_start == nullptr)
            throw SyntaxError{"End of input"};
    }

    if(compound_start->type == Token::Type::WORD && compound_start->value == "do") {
        // The `for ...; do ...; done` form
        read_command_list_into_until(get_for_command().body, {"done"});
    } else if(compound_start->type == Token::Type::WORD && compound_start->value == "{") {
        // The old `for ...; { ...; }` form
        read_command_list_into_until(get_for_command().body, {"}"});
    } else {
        throw SyntaxError{compound_start};
    }
}

// This function gets called
//     fname () [HERE] { :; }
// with `fname` already (wrongly) parsed as Command::Simple
void Parser::read_commit_function_definition()
{
    std::string function_name = get_simple_command().argv.at(0);
    m_command.value = Command::FunctionDefinition{.name = function_name};

    const Token *after_function_name = input_next_token();
    if(after_function_name == nullptr)
        throw SyntaxError{"End of input"};

    if(after_function_name->type == Token::Type::WORD && after_function_name->value == "{") {
        // The `fname() { ...; }` form
        read_command_list_into_until(command_get<Command::FunctionDefinition>().body, {"}"});
    } else {
        // Something else, like `fname() echo 1`

        // Put what we've read back
        input_put_token_back();

        throw SyntaxError{"zsh-style short functions (f() echo) are not yet supported"};
    }
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

    // For syntax highlighting:
    if(m_command.start_token == nullptr) {
        m_command.start_token = &m_input[m_input_i];
    }

    return &m_input[m_input_i++];
}

void Parser::input_put_token_back()
{
    if(m_input_i == 0) {
        throw SyntaxError{"Tried input_put_token_back on empty token"};
    }

    // For syntax highlighting:
    if(m_command.start_token == &m_input[m_input_i - 1]) {
        m_command.start_token = nullptr;
    }


    m_input_i -= 1;
}

Command::Simple &Parser::get_simple_command()
{
    if(has_empty_command()) {
        m_command.value = Command::Simple{};
    }

    if(! command_is_type<Command::Simple>()) {
        if(command_is_type<Command::BraceGroup>())
            throw SyntaxError{"{}-lists cannot take arguments"};

        if(command_is_type<Command::If>())
            throw SyntaxError{"'fi' cannot take arguments"};

        if(command_is_type<Command::While>())
            throw SyntaxError{"'done' cannot take arguments"};

        if(command_is_type<Command::Until>())
            throw SyntaxError{"'done' cannot take arguments"};

        throw SyntaxError{"Extra word"};
    }

    return command_get<Command::Simple>();
}

Command::BraceGroup &Parser::get_brace_group()
{
    if(has_empty_command()) {
        m_command.value = Command::BraceGroup{};
    }

    if(! command_is_type<Command::BraceGroup>()) {
        throw SyntaxError{"Compound commands cannot have environment variables passed to them"};
    }

    return command_get<Command::BraceGroup>();
}

Command::If &Parser::get_if_command()
{
    if(has_empty_command()) {
        m_command.value = Command::If{};
    }

    if(! command_is_type<Command::If>()) {
        if(command_is_type<Command::BraceGroup>())
            throw SyntaxError{"Missing ';' between '}' and 'if'"};

        if(command_is_type<Command::Simple>())
            throw SyntaxError{"If statements cannot have environment variables passed to them"};

        throw SyntaxError{"Unexpected if"};
    }

    return command_get<Command::If>();
}

Command::While &Parser::get_while_command()
{
    if(has_empty_command()) {
        m_command.value = Command::While{};
    }

    if(! command_is_type<Command::While>()) {
        if(command_is_type<Command::BraceGroup>())
            throw SyntaxError{"Missing ';' between '}' and 'while'"};

        if(command_is_type<Command::Simple>())
            throw SyntaxError{"While loops cannot have environment variables passed to them"};

        throw SyntaxError{"Unexpected 'while'"};
    }

    return command_get<Command::While>();
}

Command::Until &Parser::get_until_command()
{
    if(has_empty_command()) {
        m_command.value = Command::Until{};
    }

    if(! command_is_type<Command::Until>()) {
        if(command_is_type<Command::BraceGroup>())
            throw SyntaxError{"Missing ';' between '}' and 'until'"};

        if(command_is_type<Command::Simple>())
            throw SyntaxError{"Until loops cannot have environment variables passed to them"};

        throw SyntaxError{"Unexpected 'until'"};
    }

    return command_get<Command::Until>();
}

Command::For &Parser::get_for_command()
{
    if(has_empty_command()) {
        m_command.value = Command::For{};
    }

    if(! command_is_type<Command::For>()) {
        if(command_is_type<Command::BraceGroup>())
            throw SyntaxError{"Missing ';' between '}' and 'for'"};

        if(command_is_type<Command::Simple>())
            throw SyntaxError{"For loops cannot have environment variables passed to them"};

        throw SyntaxError{"Unexpected 'for'"};
    }

    return command_get<Command::For>();
}

bool Parser::has_empty_command()
{
    return command_is_type<Command::Empty>();
}

void Parser::parse_token(const Token *token) {
    // Reserved words can appear as unquoted first words of commands
    bool can_be_reserved_command = has_empty_command() && token->type == Token::Type::WORD;

    // variable assignments can appear in similar places to reserved words but there may be
    // multiple assignments per command
    bool can_be_assignment = can_be_reserved_command || (
                token->type == Token::Type::WORD
                && command_is_type<Command::Simple>()
                && command_get<Command::Simple>().argv.empty());

    // `!` and `time` can only appear at the beggining of a pipeline
    bool can_be_pipeline_prefix = can_be_reserved_command && m_pipeline.commands.empty();

    // Can operators "(" ")" start a function definition ( f() { :; } )
    bool can_be_function_definition =
            command_is_type<Command::Simple>()
            && command_get<Command::Simple>().argv.size() == 1
            && command_get<Command::Simple>().variable_assignments.size() == 0;

    if (can_be_assignment && is_token_assignment(*token)) {
        commit_assignment(token->value);
    }
    // Reserved words:
    else if (can_be_reserved_command && token->value == "{") {
        read_commit_compound_command_list();
    }
    else if (can_be_reserved_command && token->value == "if") {
        read_commit_if();
    }
    else if (can_be_reserved_command && token->value == "while") {
        read_commit_while();
    }
    else if (can_be_reserved_command && token->value == "until") {
        read_commit_until();
    }
    else if (can_be_reserved_command && token->value == "for") {
        read_commit_for();
    }
    // Pipeline prefixes:
    else if (can_be_pipeline_prefix && token->value == "!") {
        m_pipeline.negation_prefix = !m_pipeline.negation_prefix;
    }
    // Reserved words that appered in the wrong place (ones not read by read_commit_*)
    else if (can_be_reserved_command && (
             token->value == "then"
             || token->value == "elif"
             || token->value == "else"
             || token->value == "fi"
             || token->value == "do"
             || token->value == "done"
             || token->value == "esac"
             || token->value == "!")) {
        throw SyntaxError{"Unexpected token '" + token->value + "'"};
    }
    // Function definition
    else if (can_be_function_definition && token->type == Token::Type::OPERATOR && token->value == "()") {
        read_commit_function_definition();
    }
    /* TODO: `function a { :; }` and `function b() { :; }` forms */
    // Lone arguments to a command
    else if (token->type == Token::Type::WORD) {
        commit_argument(token->value, token);
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
