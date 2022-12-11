#include "completion.h"

#include "utils.h"
#include <glob.h>
#include "Global.h"
#include <string_view>
#include "Tokenizer.h"
#include "Token.h"
#include "builtins.h"

using replxx::Replxx;

namespace completion {

static void push_prefixed_completion(std::vector<Replxx::Completion> &out, std::string &&completed_word) {
    if(completed_word.empty())
        return;

    std::size_t slash = completed_word.rfind('/', completed_word.size() - 1);
    if(slash != std::string::npos)
        completed_word.erase(completed_word.begin(), completed_word.begin() + slash + 1);

    out.emplace_back(std::move(completed_word));
}

static void complete_word_path(std::vector<Replxx::Completion> &out, std::string_view word, bool getRidOfFulPathSlashes = false) {
    std::string pattern(word);
    pattern.push_back('*');

    glob_t globbuf;
    if(glob(pattern.c_str(), 0, nullptr, &globbuf) != 0) {
        return;
    }

    for(std::size_t i = 0; i < globbuf.gl_pathc; i++) {
        std::string completion(globbuf.gl_pathv[i]);
        if(getRidOfFulPathSlashes) {
            push_prefixed_completion(out, std::move(completion));
        } else {
            out.emplace_back(std::move(completion));
        }
    }

    globfree(&globbuf);
}

static void add_compl_if_matches(std::vector<Replxx::Completion> &out, std::string_view input, const std::string &word) {
    if(word.starts_with(input)) {
        out.emplace_back(word);
    }
}

static void complete_command_name_path(std::vector<Replxx::Completion> &out, std::string_view word) {
    if(std::optional<std::string> path = g.get_variable("PATH")) {
        utils::Splitter(path.value()).delim(':').for_each([&] (const std::string &dir) -> utils::Splitter::ShouldContinue {
            std::string glob_path = dir.empty() ? "." : dir;
            glob_path.push_back('/');
            glob_path.append(word);
            complete_word_path(out, glob_path, true);
            return utils::Splitter::CONTINUE_LOOP;
        });
    }
    for(const auto &builtin : *get_builtins()) {
        add_compl_if_matches(out, word, builtin.first);
    }
    for(const auto &function : g.functions) {
        add_compl_if_matches(out, word, function.first);
    }
}

std::vector<Replxx::Completion> completion_callback(Replxx &, std::string const &input, int &contextLen) {
    std::vector<Token> tokens;
    try {
        tokens = Tokenizer(input).dontThrowOnIncompleteInput().tokenize();
    } catch(const Tokenizer::SyntaxError &) {
        return {};
    }

    if(tokens.empty()) {
        return {};
    }

    // TODO: word expansion on tab completion?
    std::string_view word = tokens.back().value;

    bool isCommandName = true;

    if(utils::utf8_codepoint_len(input) > tokens.back().positionEndUtf8Codepoint) {
        word = "";
        isCommandName = true;
    } else if(tokens.back().type == Token::Type::OPERATOR) {
        word = "";
        isCommandName = false;
    } else if(word.find('/') != word.npos) {
        isCommandName = false;
    } else if(tokens.size() >= 2) {
        if(tokens.at(tokens.size() - 2).type == Token::Type::WORD) {
            std::string_view previous_word = tokens.at(tokens.size() - 2).value;
            if(previous_word != "then" &&
                    previous_word != "{" &&
                    previous_word != "(" &&
                    previous_word != "do")
            isCommandName = false;
        }
    }

    contextLen = utils::utf8_codepoint_len(word);

    std::vector<Replxx::Completion> completions;
    if(isCommandName) {
        complete_command_name_path(completions, word);
    } else {
        complete_word_path(completions, word);
    }

    std::sort(completions.begin(), completions.end(), [](const auto &a, const auto &b) {
        return a.text() < b.text();
    });

    completions.erase(std::unique(completions.begin(), completions.end(), [](const auto &a, const auto &b) {
        return a.text() == b.text();
    }), completions.end());

    return completions;
}

} // namespace completion
