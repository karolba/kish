#pragma once

#include "Parser.h"
#include <string>
#include <vector>
#include <string_view>
#include <optional>

class WordExpander {
public:
    explicit WordExpander(const std::string_view word_view)
        : m_word_view(word_view)
    { }

    bool expand_into(std::vector<std::string> &buf);
    bool expand_into_space_joined(std::string &buf);
    bool expand_into_single_word(std::string &buf);

private:
    enum State {
        FREE,
        SINGLE_QUOTED,
        DOUBLE_QUOTED,
    };

    State m_state { FREE };

    const std::string_view m_word_view;
    size_t m_word_i { 0 };

    std::vector<std::string>* m_out;

    std::string m_current_word;

    // Should en empty expended word expand to "" (`''`) or to nothing (`$empty_var`, ``)?
    bool m_current_word_can_be_empty_expansion { true };

    std::string consume_variable_name();
    void expand_variable(const std::string &variable_name, bool in_double_quotes);
    void delimit_word();

    std::optional<char> next_char();
    std::optional<char> peek_char();
};
