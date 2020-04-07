#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <sys/wait.h>
#include <functional>

namespace utils {

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

class Splitter {
public:
    enum ShouldContinue {
        CONTINUE_LOOP,
        BREAK_LOOP
    };

    Splitter(const std::string &str)
        : m_stream(str)
    {}

    Splitter& delim(char delim) {
        m_delim = delim;

        m_stream << m_delim; // std::getline ignores the last section of the split string
                             // make the last section empty to prevent this

        return *this;
    }

    template<typename T>
    std::optional<T> for_each(std::function<std::optional<T>(const std::string &part)> callback) {
        std::string part;
        while(std::getline(m_stream, part, m_delim)) {
            std::optional<T> res = callback(part);
            if(res.has_value())
                return res;
        }

        return {};
    }

    void for_each(std::function<ShouldContinue(const std::string &part)> callback) {
        std::string part;
        while(std::getline(m_stream, part, m_delim)) {
            if(callback(part) == BREAK_LOOP)
                return;
        }
    }

private:
    char m_delim { '\0' };
    std::stringstream m_stream;
};

} // namespace util
