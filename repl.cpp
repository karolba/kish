#include "repl.h"
#include <unistd.h>
#include <fstream>
#include <utility>
#include "executor.h"
#include "Global.h"
#include "highlight.h"
#include "utils.h"
#include "replxx.hxx"

#include <iostream>

using replxx::Replxx;

namespace repl {

static std::string prompt() {
    char *wd = getwd(NULL);
    if(wd == nullptr) {
        return " $ ";
    }
    std::string prompt = std::string(wd) + " $ ";
    free(wd);
    return prompt;
}

static std::optional<std::string> get_history_path() {
    auto home = g.get_variable("HOME");
    if(home.has_value()) {
        std::string path = home.value() + "/.kish_history";
        return { path };
    }
    return {};
}

static std::string read_line(Replxx &replxx) {
    char const * cinput { nullptr }; // should not be freed

    do {
        cinput = replxx.input(prompt());
    } while(cinput == nullptr && errno == EAGAIN);

    if (cinput == nullptr) {
        exit(0);
    }

    return { cinput };
}

static std::vector<Replxx::Completion> completion_callback(std::string const &input, int &contextLen) {
    (void) input;
    (void) contextLen;

    std::vector<Replxx::Completion> completions;
    completions.emplace_back(Replxx::Completion("test"));
    completions.emplace_back(Replxx::Completion("cośtam"));
    return completions;
}

static std::optional<std::string> history_command_starting_with(std::string_view str, Replxx::HistoryScan scan) {
    int limit = 1000;
    while(scan.next()) {
        if(limit-- == 0)
            return {};

        if(scan.get().text().starts_with(str)) {
            return { scan.get().text() };
        }
    }

    return {};
}

static std::vector<std::string> hint_callback(Replxx &replxx, std::string const &input, int &contextLen, Replxx::Color &) {
    if(auto command = history_command_starting_with(input, replxx.history_scan())) {
        contextLen = utils::utf8_codepoint_len(input);
        return { *command };
    }

    return {};
}

static bool is_cursor_at_end_of_line(const Replxx::State &state) {
    return state.cursor_position() == utils::utf8_codepoint_len(state.text());
}

static Replxx::ACTION_RESULT right_arrow_key_press(Replxx &replxx, char32_t code) {
    if(is_cursor_at_end_of_line(replxx.get_state())) {
        if(auto command = history_command_starting_with(replxx.get_state().text(), replxx.history_scan())) {
            replxx.set_state(Replxx::State(command->c_str(), std::numeric_limits<int>::max()));
            return Replxx::ACTION_RESULT::CONTINUE;
        }
    }

    replxx.invoke(Replxx::ACTION::MOVE_CURSOR_RIGHT, code);

    return Replxx::ACTION_RESULT::CONTINUE;
}

static void repl_loop(Replxx &replxx) {
    std::optional<std::string> history_path = get_history_path();

    if(history_path.has_value()) {
        std::ifstream history_file(history_path.value().c_str());
        replxx.history_load(history_file);
    }

    while(true) {
        std::string input = read_line(replxx);

        if(history_path.has_value()) {
            if(!input.empty()) {
                replxx.history_add(input);
            }
            replxx.history_sync(history_path.value());
        }

        executor::run_from_string(input);
    }
}

void run() {
    Replxx replxx;

    replxx.install_window_change_handler();
    replxx.set_no_color(false);
    replxx.set_highlighter_callback(highlight::highlighter_callback);
    replxx.set_completion_callback(completion_callback);
    replxx.set_hint_callback(std::bind_front(hint_callback, std::ref(replxx)));
    replxx.set_beep_on_ambiguous_completion(true);
    replxx.set_immediate_completion(true);
    replxx.bind_key(Replxx::KEY::RIGHT, std::bind_front(right_arrow_key_press, std::ref(replxx)));

    // TODO: does this improve typing latency?
    replxx.set_hint_delay(1);

    repl_loop(replxx);
}

} // namespace repl
