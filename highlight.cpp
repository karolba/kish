#include "highlight.h"
#include <iostream>

#include <algorithm>
#include <string>
#include <string_view>
#include "Parser.h"
#include "Tokenizer.h"
#include "WordExpander.h"
#include "Global.h"
#include "builtins.h"
#include "utils.h"
#include "replxx.hxx"
#include <sys/stat.h>

namespace highlight {

using replxx::Replxx;

static void highlight_commandlist(Replxx::colors_t &colors, const CommandList &cl);

static void highlight_word(Replxx::colors_t &colors, const Token *token) {
    for(int i = token->positionStartUtf8Codepoint; i < token->positionEndUtf8Codepoint; i++) {
        colors[i] = Replxx::Color::DEFAULT;
    }
}

static bool command_exists(const std::string &command_name) {
    if(g.functions.contains(command_name))
        return true;

    if(find_builtin(command_name).has_value())
        return true;

    std::optional<std::string> path = g.get_variable("PATH");
    if(path.has_value()) {
        auto exists = utils::Splitter(path.value()).delim(':').for_each<bool>([&] (const std::string &dir) -> std::optional<bool> {
            std::string command_path = (dir.empty() ? std::string{"."} : dir) + "/" + command_name;
            struct stat st;
            if(stat(command_path.c_str(), &st) != 0) {
                return {};
            }
            if(st.st_mode < 0) {
                return {};
            }
            if(! S_ISREG(st.st_mode)) {
                return {};
            }
            if(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                return { true };
            }
            return {};
        });
        if(exists.value_or(false)) {
            return true;
        }
    }

    return false;
}

static void highlight_command_simple(Replxx::colors_t &colors, const Command &command) {
    const Command::Simple &simple_command = std::get<Command::Simple>(command.value);

    for(int i = command.start_token->positionStartUtf8Codepoint; i < command.end_token->positionEndUtf8Codepoint; i++) {
        colors[i] = Replxx::Color::DEFAULT;
    }

    if(! simple_command.argv_tokens.empty()) {
        const Token *argv0 = simple_command.argv_tokens.at(0);

        std::vector<std::string> expandedArgv;

        WordExpander::Options opt;
        opt.fieldSplitting = true;
        opt.pathnameExpansion = WordExpander::Options::NEVER;
        opt.variableAtAsMultipleFields = false;
        opt.unsafeExpansions = false;
        if(! WordExpander(opt, argv0->value).expand_into(expandedArgv) || expandedArgv.empty()) {
            // if word expansion didn't succeed here - just ignore it
            expandedArgv = { argv0->value };
        }

        Replxx::Color colorOfArgv0;
        if(command_exists(expandedArgv.at(0))) {
            colorOfArgv0 = Replxx::Color::GREEN;
        } else {
            colorOfArgv0 = Replxx::Color::RED;
        }

        for(int i = argv0->positionStartUtf8Codepoint; i < argv0->positionEndUtf8Codepoint; i++) {
            colors[i] = colorOfArgv0;
        }
    }

    for(std::size_t i = 1; i < simple_command.argv_tokens.size(); i++) {
        highlight_word(colors, simple_command.argv_tokens.at(i));
    }
}

static void highlight_command_bracegroup(Replxx::colors_t &colors, const Command &command) {
    const Command::BraceGroup &bracegroup_command = std::get<Command::BraceGroup>(command.value);
    highlight_commandlist(colors, bracegroup_command.command_list);

}

static void highlight_command_if(Replxx::colors_t &colors, const Command &command) {
    const Command::If &if_command = std::get<Command::If>(command.value);
    highlight_commandlist(colors, if_command.condition);
    highlight_commandlist(colors, if_command.then);
    if(if_command.opt_else.has_value()) {
        highlight_commandlist(colors, if_command.opt_else.value());
    }
    for(const Command::If::Elif &elif : if_command.elif) {
        highlight_commandlist(colors, elif.condition);
        highlight_commandlist(colors, elif.then);
    }
}

static void highlight_command_while(Replxx::colors_t &colors, const Command &command) {
    const Command::While &while_command = std::get<Command::While>(command.value);

    highlight_commandlist(colors, while_command.condition);
    highlight_commandlist(colors, while_command.body);
}

static void highlight_command_until(Replxx::colors_t &colors, const Command &command) {
    const Command::Until &until_command = std::get<Command::Until>(command.value);

    highlight_commandlist(colors, until_command.condition);
    highlight_commandlist(colors, until_command.body);
}

static void highlight_command_for(Replxx::colors_t &colors, const Command &command) {
    const Command::For &for_command = std::get<Command::For>(command.value);

    for(const Token *item_token : for_command.items_tokens) {
        highlight_word(colors, item_token);
    }

    highlight_commandlist(colors, for_command.body);
}

static void highlight_command_functiondefinition(Replxx::colors_t &colors, const Command &command) {
    const Command::FunctionDefinition &function_definition_command = std::get<Command::FunctionDefinition>(command.value);

    highlight_commandlist(colors, function_definition_command.body);
}

static void highlight_command(Replxx::colors_t &colors, const Command &command) {
    // unhighlighted pieces of a command - default to magenta
    for(int i = command.start_token->positionStartUtf8Codepoint; i < command.end_token->positionEndUtf8Codepoint; i++) {
        colors[i] = Replxx::Color::BRIGHTMAGENTA;
    }

    std::visit(utils::overloaded {
          [&] (const Command::Empty) { },
          [&] (const Command::Simple) { highlight_command_simple(colors, command); },
          [&] (const Command::BraceGroup) { highlight_command_bracegroup(colors, command); },
          [&] (const Command::If) { highlight_command_if(colors, command); },
          [&] (const Command::While) { highlight_command_while(colors, command); },
          [&] (const Command::Until) { highlight_command_until(colors, command); },
          [&] (const Command::For) { highlight_command_for(colors, command); },
          [&] (const Command::FunctionDefinition) { highlight_command_functiondefinition(colors, command); }
    }, command.value);

    for(const Redirection &redir : command.redirections) {
        if(redir.filename_token.has_value()) {
            highlight_word(colors, *redir.filename_token);
        }
    }
}

static void highlight_pipeline(Replxx::colors_t &colors, const Pipeline &pipeline) {
    for(const Command &command : pipeline.commands) {
        highlight_command(colors, command);
    }
}

static void highlight_and_or_list(Replxx::colors_t &colors, const AndOrList &and_or_list) {
    for(const WithFollowingOperator<Pipeline> &pipe_op : and_or_list) {
        highlight_pipeline(colors, pipe_op.val);
    }
}

static void highlight_commandlist(Replxx::colors_t &colors, const CommandList &cl) {
    for(const WithFollowingOperator<AndOrList> &aol_op : cl) {
        highlight_and_or_list(colors, aol_op.val);
    }
}

static void highlight_everything_by_tokens(std::string const &input, Replxx::colors_t &colors, std::vector<Token> &tokens) {
    int utf8_index = -1;
    int byte_index = -1;
    int token_index = -1;
    for(char c : input) {
        byte_index += 1;
        // utf-8 multi-byte character - front part
        if(utils::front_of_multibyte_utf8_codepoint(c)) {
            continue;
        }
        utf8_index += 1;

        if(token_index + 1 < static_cast<int>(tokens.size())) {
            if(byte_index >= tokens.at(token_index + 1).positionStart) {
                token_index += 1;
            }
        }

        if(token_index == -1) {
            // empty command line - or a comment followed by nothing or lone whitespace
            colors[utf8_index] = Replxx::Color::GRAY;
        } else if(tokens.at(token_index).positionEnd < byte_index) {
            // a comment after some nonwhitespace text
            colors[utf8_index] = Replxx::Color::GRAY;
        } else if(tokens.at(token_index).type == Token::Type::OPERATOR) {
            // <, or >>, or |, or ...
            colors[utf8_index] = Replxx::Color::BRIGHTCYAN;
        } else if(tokens.at(token_index).type == Token::Type::WORD) {
            // might get everriden by highlight_command_list - but set the default
            //colors[utf8_index] = Replxx::Color::DEFAULT;
            // no longer do this - tokens go last now for easier implementations
        }
    }

}

static void highlight_all_red(std::string const &input, Replxx::colors_t &colors) {
    // syntax error - make everything red
    int i = 0;
    for(char c : input) {
        // utf-8 multi-byte character - front part
        if(utils::front_of_multibyte_utf8_codepoint(c)) {
            continue;
        }

        colors[i++] = Replxx::Color::BRIGHTRED;
    }
}

void highlighter_callback(std::string const &input, Replxx::colors_t &colors) {
    std::vector<Token> tokens;
    CommandList parsed;
    try {
        tokens = Tokenizer(input).tokenize();
        parsed = Parser(tokens).parse();
    } catch(const Tokenizer::SyntaxError &) {
        highlight_all_red(input, colors);
        return;
    } catch(const Parser::SyntaxError &) {
        highlight_all_red(input, colors);
        return;
    }

    highlight_commandlist(colors, parsed);
    highlight_everything_by_tokens(input, colors, tokens);
}

} // namespace highlight
