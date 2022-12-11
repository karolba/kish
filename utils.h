#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <sys/wait.h>
#include <functional>
#include <optional>
#include <ranges>
#include <algorithm>
#include <string_view>
#include <iostream>
#include <iomanip>
#include "Global.h"


namespace utils {

// strchr(3) matches '\0' as always belonging to the string, because
// "The terminating null character is considered to be part of the string; therefore if c
//  is ‘\0’, the functions locate the terminating ‘\0’."
// This function prevents that from happening
inline char *strchr_no_null(const char *p, char ch) {
    while(*p) {
        if (*p == ch)
            return (char *)p;
        p++;
    }
    return nullptr;
}

/*
 * Don't use isdigit(3) or isalpha(3) from the standard library, because on glibc these functions
 * needlessly depend on the current locale
 */
inline bool no_locale_isdigit(char ch) {
    return ch >= '0' && ch <= '9';
}
inline bool no_locale_isalpha(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}
inline bool no_locale_isalnum(char ch) {
    return no_locale_isalpha(ch) || no_locale_isdigit(ch);
}

// Helper class for inline std::visit invocation
// Example usage:
//          std::visit(utils::overloaded {
//              [](auto arg) { std::cout << arg << ' '; },
//              [](double arg) { std::cout << std::fixed << arg << ' '; },
//              [](const std::string& arg) { std::cout << std::quoted(arg) << ' '; },
//          }, v);
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>; // not needed as of C++20
// Credit goes to https://en.cppreference.com/w/cpp/utility/variant/visit


void wait_for_one(pid_t pid);
void wait_for_all(const std::vector<int> &pids);

std::istream &getline_multiple_delimeters(std::istream &in, std::string &out, std::string_view delim);

class Splitter {
public:
    enum ShouldContinue {
        CONTINUE_LOOP,
        BREAK_LOOP
    };

    Splitter(const std::string &str)
        : m_stream(str)
    {}

    Splitter &delim(char delim) {
        m_delim = { delim };

        m_stream << m_delim; // std::getline ignores the last section of the split string
                             // make the last section empty to prevent this

        return *this;
    }

    Splitter &delim(std::string_view delim) {
        m_delim = delim;

        if(delim.size() > 0) {
            m_stream << m_delim.at(0); // std::getline ignores the last section of the split string
                                       // make the last section empty to prevent this
        }

        return *this;
    }

    Splitter &delimIFS() {
        std::string IFS = g.get_variable("IFS").value_or("");

        if(IFS.empty())
            m_delim = " \t\n";
        else
            m_delim = IFS;

        return *this;
    }

    bool correct_getline(std::string &part) {
        if(m_delim.size() == 0) {
            part = m_stream.str();
            return false;
        } else if(m_delim.size() == 1 && m_delim.at(0) != ' ' && m_delim.at(0) != '\t' && m_delim.at(0) != '\n') {
            return !!std::getline(m_stream, part, m_delim.at(0));
        } else {
            return !!getline_multiple_delimeters(m_stream, part, m_delim);
        }
    }

    template<typename T>
    std::optional<T> for_each(std::function<std::optional<T>(const std::string &part)> callback) {
        std::string part;
        while(correct_getline(part)) {
            std::optional<T> res = callback(part);
            if(res.has_value())
                return res;
        }

        return {};
    }

    void for_each(std::function<ShouldContinue(const std::string &part)> callback) {
        std::string part;
        while(correct_getline(part)) {
            if(callback(part) == BREAK_LOOP)
                return;
        }
    }

    std::string rest_of_input() {
        if(m_stream.str().size() <= (std::size_t)m_stream.tellg())
            return "";
        return m_stream.str().substr(m_stream.tellg());
    }

private:
    std::string m_delim { '\0' };
    std::stringstream m_stream;
};



inline bool front_of_multibyte_utf8_codepoint(char c) {
    return (c & 0b1000'0000 && !(c & 0b0100'0000));
}

inline int utf8_codepoint_len(std::string_view s, int end) {
    int len = 0;
    for(int i = 0; i < end; i++) {
        if(front_of_multibyte_utf8_codepoint(s[i])) {
            continue;
        }
        len += 1;
    }
    return len;
}

inline int utf8_codepoint_len(std::string_view s) {
    return utf8_codepoint_len(s, s.size());
}

std::string_view remove_utf8_prefix(std::string_view view, std::size_t prefix);

std::string common_prefix(const std::vector<std::string> &strings);

} // namespace util
