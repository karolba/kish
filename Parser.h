#pragma once

#include "Token.h"

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <string_view>
#include <deque>
#include <memory>

// TODO: After C++20 rolls out, switch to std::span
#include "tcb/span.hpp"

// TODO: Move into Command::Redirection
struct Redirection {
    enum Type {
        FileWrite, // 1>file
        FileWriteAppend, // 1>>file
        FileRead, // 0<file
        Rewiring, // 1>&2
    };
    Type type;
    int fd { -1 };
    int rewire_fd { -1 };
    std::string path {};

    // nullopt if type == Type::Rewiring
    std::optional<const Token *> filename_token = std::nullopt;
};

template <typename T>
struct WithFollowingOperator {
    T val;
    std::string following_operator {};
};

struct Command;

// POSIX: "A pipeline is a sequence of one or more commands separated by the control operator '|'."
struct Pipeline {
    std::vector<Command> commands;
    bool negation_prefix = false; // `! a | b`
};

// POSIX: "An AND-OR list is a sequence of one or more pipelines separated by the operators "&&" and "||"."
using AndOrList = std::vector<WithFollowingOperator<Pipeline>>;

// POSIX: "A list is a sequence of one or more AND-OR lists separated by the operators ';' and '&'."
using CommandList = std::vector<WithFollowingOperator<AndOrList>>;

struct Command {
    using Empty = std::monostate;
    struct Simple { // [a=b] cmd [arg1] [arg2]
        struct VariableAssignment {
            std::string name {};
            std::string value {};
        };

        std::vector<VariableAssignment> variable_assignments; // TODO: this should be a smart pointer
        std::vector<std::string> argv;

        // For syntax highlighting
        std::vector<const Token*> argv_tokens;
    };
    struct BraceGroup { // `{ true; false; }`
        CommandList command_list;
    };
    // TODO: Technically `if` and others are compound commands too,
    // Command::Compound should be renamed to something like Command::SimpleCompound
    struct If {
        struct Elif {
            CommandList condition;
            CommandList then;
        };

        CommandList condition;
        CommandList then;
        std::vector<Elif> elif;
        std::optional<CommandList> opt_else;
    };
    struct While {
        CommandList condition;
        CommandList body;
    };
    struct Until {
        CommandList condition;
        CommandList body;
    };
    struct For { // for varname [in WORD...]; do ...
        std::string varname;
        std::vector<std::string> items;
        CommandList body;

        // For syntax highlighting
        std::vector<const Token*> items_tokens;
    };
    struct FunctionDefinition {
        std::string name;
        CommandList body;
    };

    // TODO: to prevent unneccessary copying in run_pipeline:
    std::deque<Redirection> redirections; // TODO: this should be a smart pointer

    // File descriptors to close in the main shell process after forking
    std::deque<int> pipe_file_descriptors;

    // For syntax highlighting: keep track where a command starts and ends
    // those pointers will only live as long as the tcb::span<const Token> input lives
    const Token *start_token = nullptr;
    const Token *end_token = nullptr;

    std::variant<Empty, Simple, BraceGroup, If, While, Until, For, FunctionDefinition> value;
};

class Parser {
public:
    explicit Parser(tcb::span<const Token> input)
        : m_input(input)
    {}

    CommandList parse();

    struct SyntaxError {
        SyntaxError(const Token *tok)
            : explanation("Unexpected '" + tok->value + "'") // TODO: quoted(tok->value)
        {}
        SyntaxError(const std::string &explanation)
            : explanation(explanation)
        {}
        std::string explanation;
    };
private:
    Command m_command;
    Pipeline m_pipeline;
    AndOrList m_and_or_list;
    CommandList m_command_list;

    tcb::span<const Token> m_input;

    size_t m_input_i { 0 };
    const Token *input_next_token();
    void input_put_token_back();

    Command::Simple &get_simple_command();
    Command::BraceGroup &get_brace_group();
    Command::If &get_if_command();
    Command::While &get_while_command();
    Command::Until &get_until_command();
    Command::For &get_for_command();

    bool has_empty_command();

    void parse_token(const Token *token);

    void commit_assignment(const std::string &assignment);
    void commit_argument(const std::string &word, const Token *token_for_highlighting);
    void commit_redirection(const std::string &op);

    void read_commit_compound_command_list();
    void read_commit_if();
    void read_commit_while();
    void read_commit_until();
    void read_commit_for();
    void read_commit_function_definition();

    void for_loop_add_item(const Token *token);

    const Token *read_command_list_into_until(CommandList& into, const std::vector<std::string_view> &until_command);

    void commit_command();
    void commit_pipeline(const Token* op = nullptr);
    void commit_and_or_list(const Token* op = nullptr);

    std::tuple<CommandList, size_t, const Token *> parse_until(const std::vector<std::string_view> &command);

    template <typename T>
    bool command_is_type() {
        return std::holds_alternative<T>(m_command.value);
    }

    template <typename T>
    T& command_get() {
        return std::get<T>(m_command.value);
    }
};

