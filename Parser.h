#pragma once

#include "Token.h"

#include <string>
#include <vector>
#include <variant>
#include <optional>


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
};

struct VariableAssignment {
    std::string name {};
    std::string value {};
};


template <typename T>
struct WithFollowingOperator {
    T val;
    std::string following_operator {};
};

struct Command;
// POSIX: "A pipeline is a sequence of one or more commands separated by the control operator '|'."
using Pipeline = std::vector<Command>;

// POSIX: "An AND-OR list is a sequence of one or more pipelines separated by the operators "&&" and "||"."
using AndOrList = std::vector<WithFollowingOperator<Pipeline>>;

// POSIX: "A list is a sequence of one or more AND-OR lists separated by the operators ';' and '&'."
using CommandList = std::vector<WithFollowingOperator<AndOrList>>;

struct Command {
    using Empty = std::monostate;
    struct Simple {
        std::vector<VariableAssignment> variable_assignments; // TODO: this should be a smart pointer
        std::vector<std::string> argv;
    };
    struct Compound { // `{ true; false; }`
        CommandList command_list;
    };

    // TODO: to prevent unneccessary copying in run_pipeline:
    std::vector<Redirection> redirections; // TODO: this should be a smart pointer

    // File descriptors to close in the main shell process after forking
    std::vector<int> pipe_file_descriptors;

    std::variant<Empty, Simple, Compound> value;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& input)
        : m_input(input)
    {}

    CommandList parse();

    struct SyntaxError { std::string explanation; };
private:
    Command m_command;
    Pipeline m_pipeline;
    AndOrList m_and_or_list;
    CommandList m_command_list;

    std::vector<Token> m_input; // TODO: this probably doesn't need to be a copy

    size_t m_input_i { 0 };
    Token* input_next_token();

    Command::Simple *get_simple_command();
    Command::Compound *get_compound_command();

    bool has_empty_command();

    void parse_token(Token *token);

    void commit_assignment(const std::string &assignment);
    void commit_argument(const std::string &word);
    void commit_redirection(const std::string &op);

    void read_commit_compound_command_list();

    void commit_command();
    void commit_pipeline(const Token* op = nullptr);
    void commit_and_or_list(const Token* op = nullptr);

    std::pair<CommandList, size_t> parse_compound_command_list();
};
