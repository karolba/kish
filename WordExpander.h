#pragma once

#include "Parser.h"
#include <string>
#include <vector>
#include <string_view>
#include <optional>

class WordExpander {
public:
    struct Options {
        // Do tilde expansion, parameter expansion, command substitution, and arithmetic expansion
        // Being able to disable this is useful in `cat << "EOF"` where we only want quote removal
        bool commonExpansions = false;

        // Should "$@" expand to multiple fields or behave like "$*" with IFS=' '
        // (used for variable assignments, where (with IFS=' ') `var="$*"` === `var="$@"`)
        bool variableAtAsMultipleFields = false;

        // Do field splitting according to $IFS
        bool fieldSplitting = false;

        // Do pathname expansion (like in `echo *`)
        enum { ALWAYS, NEVER, ONLY_IF_SINGLE_RESULT } pathnameExpansion = NEVER;

        // If set, stop when the given unquoted char is encountered.
        // When set to ")"  [TODO: ...]
        std::optional<char> expandUntilUnquotedChar;
    };

    explicit WordExpander(const Options &opt, std::string_view input)
        : opt(opt)
        , input(input)
    { }

    bool expand_into(std::vector<std::string> &buf);

    // Expands into a single string
    // if the result of the word expansion consists of multiple fields, returns false
    bool expand_into(std::string &buf);

private:
    Options opt;
    std::string_view input;
    std::vector<std::string> *out;
    std::vector<size_t> pathname_expansion_pattern_location;

    void add_character_literal(char ch);
    void add_character_unquoted(char ch);
    void add_character_quoted(char ch);

    void mark_pathname_expansion_character_location();

    void delimit_by_whitespace();
    void delimit_by_non_whitespace();

    void do_pathname_expansion();

    size_t expand_tilda(size_t input_position);
    size_t expand_command_substitution_free(size_t input_position);
    size_t expand_command_substitution_double_quoted(size_t input_position);
    void expand_special_variable_free(char varname);
    void expand_special_variable_double_quoted(char varname);
    size_t expand_variable_free(size_t variable_name_begin);
    size_t expand_variable_double_quoted(size_t variable_name_begin);
};
