#include "WordExpander.h"
#include <ctype.h>
#include <string>
#include <string.h>
#include "Global.h"

#include <pwd.h>
#include <errno.h>

#include <iostream>

static bool is_one_letter_variable_name(char ch) {
    return isdigit(ch) || strchr("!@#$?-", ch) != nullptr;
}

static bool can_start_variable_name(char ch) {
    return isalpha(ch) || ch == '_';
}

static bool can_be_in_variable_name(char ch) {
    return isalnum(ch) || ch == '_';
}

bool WordExpander::expand_into(std::vector<std::string> &buf)
{
    out = &buf;

    // TODO: Do these lines need to be changed to support $IFS?
    if(buf.size() != 0 && buf.back().size() != 0)
        buf.emplace_back();

    enum { FREE, SINGLE_QUOTED, DOUBLE_QUOTED } state = FREE;

    for(size_t i = 0; i < input.size(); i++) {
        char ch = input[i];

        std::optional<char> next_ch, prev_ch;
        if(i != input.size() - 1)
            next_ch = input[i + 1];
        if(i != 0)
            prev_ch = input[i - 1];

        if(state == FREE && ch == '"') {
            state = DOUBLE_QUOTED;
        } else if(state == FREE && ch == '\'') {
            state = SINGLE_QUOTED;
        } else if(state == DOUBLE_QUOTED && ch == '"') {
            state = FREE;
        } else if(state == SINGLE_QUOTED && ch == '\'') {
            state = FREE;
        } else if((state == FREE || state == DOUBLE_QUOTED) && ch == '\\') {
            if(next_ch)
                add_character_quoted(next_ch.value());
            i += 1;
        } else if(state == FREE && ch == '$' && next_ch.has_value() && is_one_letter_variable_name(next_ch.value())) {
            expand_special_variable_free(next_ch.value());
            i += 1;
        } else if(state == DOUBLE_QUOTED && ch == '$' && next_ch.has_value() && is_one_letter_variable_name(next_ch.value())) {
            expand_special_variable_double_quoted(next_ch.value());
            i += 1;
        } else if(state == FREE && ch == '$' && can_start_variable_name(next_ch.value_or('\0'))) {
            i = expand_variable_free(i + 1);
        } else if(state == DOUBLE_QUOTED && ch == '$' && can_start_variable_name(next_ch.value_or('\0'))) {
            i = expand_variable_double_quoted(i + 1); // TODO: add a test that fails if this does expand_variable_free
        } else if(state == FREE && ch == '$' && next_ch.value_or('\0') == '(') {
            i = expand_command_substitution_unquoted(i + 1);
        } else if(state == DOUBLE_QUOTED && ch == '$' && next_ch.value_or('\0') == '(') {
            i = expand_command_substitution_double_quoted(i + 1);
        } else if(state == FREE && ch == '~' && (!prev_ch.has_value() || prev_ch.value() == ':')) {
            i = expand_tilda(i + 1);
        } else if(state == SINGLE_QUOTED) {
            add_character_quoted(ch);
        } else if(state == DOUBLE_QUOTED) {
            add_character_quoted(ch);
        } else if(state == FREE) {
            add_character_literal(ch);
        }
    }

    do_pathname_expansion();

    return true;
}

bool WordExpander::expand_into(std::string &buf)
{
    // TODO: avoid a copy here somehow
    std::vector<std::string> buf_vec;
    expand_into(buf_vec);

    if(buf_vec.size() > 1) {
        return false;
    }

    if(buf_vec.size() == 0) {
        buf = "";
        return true;
    }

    buf = buf_vec.at(0);
    return true;
}

// Used for expanded ~ and literals
void WordExpander::add_character_literal(char ch)
{
    // This should not conform to $IFS
    if(ch == ' ' || ch == '\t' || ch == '\n') {
        delimit_by_whitespace();
        return;
    }

    add_character_quoted(ch);

    if(ch == '*' || ch == '?') {
        mark_pathname_expansion_character_location();
    }
}

// Used for expanded unquoted strings (like $())
void WordExpander::add_character_unquoted(char ch)
{
    /* TODO: Implement $IFS here */
    if(ch == ' ' || ch == '\t' || ch == '\n') {
        delimit_by_whitespace();
        return;
    }

    add_character_quoted(ch);

    if(ch == '*' || ch == '?') {
        mark_pathname_expansion_character_location();
    }
}

void WordExpander::add_character_quoted(char ch)
{
    if(out->size() == 0)
        out->push_back({});

    out->back().push_back(ch);
}

void WordExpander::mark_pathname_expansion_character_location()
{
    size_t current_location = out->back().size() - 1;
    pathname_expansion_pattern_location.push_back(current_location);
}

void WordExpander::delimit_by_whitespace()
{
    // TODO: Handle IFS=', '
    // "a , b" -> {"a", "b"}

    // If delimiting by whitespace, don't delimit multiple times if there's adjoining whitespace
    // For example, "a  b" -> {"a", "b"}
    if(out->size() == 0 || out->back().size() == 0)
        return;

    delimit_by_non_whitespace();
}

void WordExpander::delimit_by_non_whitespace()
{
    // Delimiting by non-whitespace (with $IFS) should result in empty fields in case of repeated delimeters
    // For example, "a::b" -> {"a", "", "b"}
    do_pathname_expansion();

    out->emplace_back();
}

void WordExpander::do_pathname_expansion()
{
    if(!pathname_expansion_pattern_location.empty()) {
//        std::cerr << "Word with pathname expansion: [" << out->back() << "]\n";
    } else {
//        std::cerr << "Word without pathname expansion: [" << out->back() << "]\n";
    }

    pathname_expansion_pattern_location.clear();
}

size_t WordExpander::expand_tilda(size_t username_begin)
{
    size_t username_end = username_begin;
    while(username_end < input.size() && (isalnum(input[username_end]) || strchr("._-", input[username_end]) != nullptr)) {
        username_end++;
    }

    std::string username = std::string(input.substr(username_begin, username_end - username_begin));

    std::string_view expanded;
    passwd *pw;
    std::optional<std::string> HOME;

    if(username.empty()) {
        // Expanding a lonely "~"
        HOME = g.get_variable("HOME");

        // If $HOME somehow isn't set, put back a tilde
        static const char* static_tilde = "~";
        expanded = HOME.value_or(static_tilde);
    } else {
        // Expanding the longer "~user"

        // Retry getpwnam as many times as neccessary if interrupted
        do {
            errno = 0;
            pw = getpwnam(username.c_str());
        } while(pw == nullptr && errno == EINTR);

        if(pw == nullptr) {
            // On different systems getpwnam sets errno to 0 or ENOENT or ESRCH or EBADF or EPERM or ...
            // when the given name could not be found.
            // Those are the only real errors calling getpwnam can result in:
            if(errno == EIO || errno == EMFILE || errno == ENFILE || errno == ENOMEM) {
                perror("kish: getpwnam");
            }

            // No matching username found: Put the original "~user" expression back as an expanded expression
            expanded = input.substr(username_begin - 1, username_end - username_begin + 1);
        } else {
            // Successfully got the home directory of ~user
            expanded = pw->pw_dir;
        }
    }

    if(out->size() == 0) {
        out->emplace_back(expanded);
    } else {
        out->back().append(expanded);
    }

    return username_end - 1;
}

size_t WordExpander::expand_command_substitution_unquoted(size_t input_position)
{
    // TODO: this function

    Options opt;
    opt.commonExpansions = true;
    opt.variableAtAsMultipleFields = false;
    opt.fieldSplitting = false;
    opt.pathnameExpansion = Options::ALWAYS;

    std::string expanded;
    WordExpander(opt, input.substr(input_position)).expand_into(expanded);

    for(char ch : expanded) {
        add_character_unquoted(ch);
    }

    return input_position;
}

size_t WordExpander::expand_command_substitution_double_quoted(size_t input_position)
{
    // TODO: this function

    Options opt;
    opt.commonExpansions = true;
    opt.variableAtAsMultipleFields = false;
    opt.fieldSplitting = false;
    opt.pathnameExpansion = Options::ALWAYS;

    std::string expanded;
    WordExpander(opt, input.substr(input_position)).expand_into(expanded);

    if(out->size() == 0) {
        out->emplace_back(expanded);
    } else {
        out->back().append(expanded);
    }

    return input_position;
}

void WordExpander::expand_special_variable_free(char varname)
{
    std::optional<std::string> var_value = g.get_variable(std::string(1, varname));
    if(var_value.has_value()) {
        for(char ch : var_value.value()) {
            add_character_unquoted(ch);
        }
    }
}

void WordExpander::expand_special_variable_double_quoted(char varname)
{
    std::optional<std::string> var_value = g.get_variable(std::string(1, varname));
    if(var_value.has_value()) {
        if(out->size() == 0) {
            out->emplace_back(var_value.value());
        } else {
            out->back().append(var_value.value());
        }
    }
}

size_t WordExpander::expand_variable_free(size_t variable_name_begin)
{
    size_t variable_name_end = variable_name_begin;
    while(variable_name_end < input.size() && can_be_in_variable_name(input[variable_name_end])) {
        variable_name_end++;
    }

    std::string variable_name = std::string(input.substr(variable_name_begin, variable_name_end - variable_name_begin));

    std::optional<std::string> variable_value = g.get_variable(variable_name);

    if(variable_value.has_value()) {
        for(char ch : variable_value.value()) {
            add_character_unquoted(ch);
        }
    }

    return variable_name_end - 1;
}

size_t WordExpander::expand_variable_double_quoted(size_t variable_name_begin)
{
    size_t variable_name_end = variable_name_begin;
    while(variable_name_end < input.size() && can_be_in_variable_name(input[variable_name_end])) {
        variable_name_end++;
    }

    std::string variable_name = std::string(input.substr(variable_name_begin, variable_name_end - variable_name_begin));

    std::optional<std::string> variable_value = g.get_variable(variable_name);

    if(variable_value.has_value()) {
        if(out->size() == 0) {
            out->emplace_back(variable_value.value());
        } else {
            out->back().append(variable_value.value());
        }
    }

    return variable_name_end - 1;
}
