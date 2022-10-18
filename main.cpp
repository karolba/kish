#include <string.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include "Tokenizer.h"
#include "Global.h"
#include "Parser.h"
#include "replxx.hxx"
#include "executor.h"

using replxx::Replxx;

static void initialize_variables() {
    // "In a subshell, '$' shall expand to the same value as that of the current shell."
    g.variables["$"] = std::to_string(getpid());
}

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

static void highlighter_callback(std::string const& input, Replxx::colors_t& colors) {
    std::vector<Token> tokens = Tokenizer(input).tokenize();
    CommandList parsed;
    try {
        parsed = Parser(tokens).parse();
    } catch(const Parser::SyntaxError &) {
        // syntax error - make everything red
        int i = 0;
        for(char c : input) {
            // utf-8 multi-byte character - front part
            if(c & 0b1000'0000 && !(c & 0b0100'0000)) {
                continue; // TODO: test this
            }

            colors[i++] = Replxx::Color::BRIGHTRED;
        }

        return;
    }

    int i = 0;
    for(char c : input) {
        // utf-8 multi-byte character - front part
        if(c & 0b1000'0000 && !(c & 0b0100'0000)) {
            continue; // TODO: test this
        }

        colors[i++] = Replxx::Color::BRIGHTGREEN;
    }

}

static std::string read_line(Replxx *replxx) {
    char const * cinput { nullptr }; // should not be freed

    do {
        cinput = replxx->input(prompt());
    } while(cinput == nullptr && errno == EAGAIN);

    if (cinput == nullptr) {
        exit(0);
    }

    return { cinput };
}

static void run_repl() {
    std::optional<std::string> history_path = get_history_path();

    Replxx *replxx = new Replxx;

    replxx->install_window_change_handler();
    replxx->clear_screen();
    replxx->set_no_color(false);
    replxx->set_highlighter_callback(highlighter_callback);
    replxx->set_beep_on_ambiguous_completion(true);

    if(history_path.has_value()) {
        std::ifstream history_file(history_path.value().c_str());
        replxx->history_load(history_file);
    }

    // repl loop
    while(true) {
        std::string input = read_line(replxx);

        if(history_path.has_value()) {
            if(!input.empty()) {
                replxx->history_add(input);
            }
            replxx->history_sync(history_path.value());
        }

        run_from_string(input);
    }

    delete replxx;
}

static void usage(const char *ownName) {
    std::cerr << "Usage: " << ownName << "\n"
              << "   or: " << ownName << " -c <command>\n"
              << "   or: " << ownName << " <scriptfile>\n";
}

int main(int argc, char *argv[]) {
    initialize_variables();

    if(argc == 1) {
        run_repl();
    } else if(argc == 2 && argv[1][0] != '-') {
        // TODO: make this efficiant
        // TODO: don't save the whole file at all
        std::string lines;
        std::ifstream f(argv[1]);
        std::string line;
        while(std::getline(f, line)) {
            lines.append(line);
            lines.append("\n");
        }
        run_from_string(lines);
    } else if(argc == 3 && strcmp(argv[1], "-c") == 0) {
        run_from_string(argv[2]);
    } else {
        usage(argc > 0 ? argv[0] : "kish");
        return 1;
    }

    return g.last_return_value;
}

#if 0
    // old repl:
    std::string input_line;
    while(echo_prompt() && std::getline(std::cin, input_line)) {
        run_from_string(input_line);
    }
#endif
