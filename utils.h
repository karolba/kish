#pragma once
#include <vector>
#include <string>
#include <sys/wait.h>

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

} // namespace util
