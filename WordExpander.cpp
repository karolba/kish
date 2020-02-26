#include "WordExpander.h"
#include <ctype.h>
#include <string>
#include <string.h>
#include "Global.h"


bool WordExpander::expand_into(std::vector<std::string> &out)
{
    //dbgprintf("expand_into()\n");
    m_current_word.clear();
    //dbgprintf("expand_into(): after clear\n");

    m_out = &out;
    //m_out->append(m_word_view);
    //return true;

    // TODO: difference between "" and $(asd)
    // and "$a" vs $a

    if (m_word_view.length() == 0)
        return true;

    std::optional<char> opt_ch;
    while ((opt_ch = next_char())) {
        char ch = opt_ch.value();

        if(m_state == SINGLE_QUOTED && ch == '\'') {
            m_state = FREE;
        }
        else if(m_state == SINGLE_QUOTED && ch != '\'') {
            m_current_word.push_back(ch);
        }
        else if(m_state == DOUBLE_QUOTED && ch == '$') {
            // TODO: ${}, $() and $(())
            std::string var_name = consume_variable_name();
            if (! var_name.empty()) {
                expand_variable(var_name, true);
            } else {
                m_current_word.push_back(ch);
            }
        }
        else if(m_state == DOUBLE_QUOTED && ch == '"') {
            m_state = FREE;
        }
        else if(m_state == FREE && ch == '\'') {
            m_current_word_can_be_empty_expansion = false;
            m_state = SINGLE_QUOTED;
        }
        else if(m_state == FREE && ch == '"') {
            m_current_word_can_be_empty_expansion = false;
            m_state = DOUBLE_QUOTED;
        }
        else if((m_state == FREE || m_state == DOUBLE_QUOTED) && ch == '\\') {
            auto next = next_char();
            if(!next)
                return false;
            m_current_word.push_back(next.value());
        }
        else if(m_state == FREE && ch == '$') {
            // TODO: ${}, $() and $(())
            std::string var_name = consume_variable_name();
            if (! var_name.empty()) {
                expand_variable(var_name, true);
            } else {
                m_current_word.push_back(ch);
            }
        }
        else if(m_state == FREE && strchr(" \t\r\n", ch) != nullptr) {
            delimit_word();
        }
        else {
            // TODO: make this a method?
            m_current_word.push_back(ch);
        }
    }

    // TODO: '' vs $a ?

    delimit_word();

    return true;
}

bool WordExpander::expand_into_space_joined(std::string &buf)
{
    std::vector<std::string> expanded;
    if(!expand_into(expanded))
        return false;

    bool first = true;
    for(const auto &word : expanded) {
        if(!first)
            buf += ' ';
        buf += word;
        first = false;
    }

    return true;
}

bool WordExpander::expand_into_single_word(std::string &buf)
{
    std::vector<std::string> expanded;
    if(!expand_into(expanded))
        return false;

    if(expanded.size() != 1)
        return false;

    buf = expanded.at(0);
    return true;
}

std::string WordExpander::consume_variable_name()
{
    std::string name;
    if (peek_char().has_value() && (isdigit(peek_char().value()) || strchr("!@#$*-?", peek_char().value()) != nullptr)) {
        name.push_back(next_char().value());
        return name;
    }
    while (peek_char().has_value() && (isalnum(peek_char().value()) || peek_char().value() == '_')) {
        name.push_back(next_char().value());
    }
    return name;
}

void WordExpander::expand_variable(const std::string &variable_name, bool in_double_quotes)
{
    std::string value = g.get_variable(variable_name).value_or("");
    if (value.empty())
        return;

    if (in_double_quotes) {
        for (size_t i = 0; i < value.length(); ++i)
            m_current_word.push_back(value[i]);
    } else {
        bool previous_was_whitespace = true;
        for (size_t i = 0; i < value.length(); ++i) {
            bool is_whitespace = strchr(" \t\r\n", value[i]) != nullptr;
            if (is_whitespace && !previous_was_whitespace) {
                delimit_word();
            }
            if (!is_whitespace) {
                m_current_word.push_back(value[i]);
            }
            previous_was_whitespace = is_whitespace;
        }
    }
}

void WordExpander::delimit_word()
{
    if (m_current_word.length() != 0 || m_current_word_can_be_empty_expansion) {
        m_out->push_back(m_current_word);
        m_current_word.clear();
    }
    m_current_word_can_be_empty_expansion = true;
}

std::optional<char> WordExpander::next_char()
{
    if (m_word_i >= m_word_view.length()) {
        return {};
    }

    return { m_word_view[m_word_i++] };
}

std::optional<char> WordExpander::peek_char()
{
    if (m_word_i >= m_word_view.length()) {
        return {};
    }

    return { m_word_view[m_word_i] };
}

