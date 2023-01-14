#include <string>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include "Global.h"
#include "executor.h"
#include "repl.h"
#include "job_control.h"


static void initialize_variables() {
    // "In a subshell, '$' shall expand to the same value as that of the current shell."
    g.variables["$"] = std::to_string(getpid());
}

static void load_kishrc() {
    std::string home;

    if(const char *c_home = getenv("HOME")) {
        home = c_home;
    } else {
        return;
    }

    // TODO: make this efficiant
    // TODO: don't save the whole file at all
    std::string lines;
    std::ifstream f(home + "/.kishrc");
    std::string line;
    while(std::getline(f, line)) {
        lines.append(line);
        lines.append("\n");
    }
    executor::run_from_string(lines);
}

static void usage(const char *ownName) {
    std::cerr << "Usage: " << ownName << "\n"
              << "   or: " << ownName << " -c <command>\n"
              << "   or: " << ownName << " <scriptfile>\n";
}

int main(int argc, char *argv[]) {
    // The replxx library uses C stdio - do this to not get confused
    std::ios::sync_with_stdio();

    initialize_variables();

    job_control::init_interactive_shell();
    if(argc == 1) {
        load_kishrc();
        repl::run();
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
        executor::run_from_string(lines);
    } else if(argc == 3 && strcmp(argv[1], "-c") == 0) {
        executor::run_from_string(argv[2]);
    } else {
        usage(argc > 0 ? argv[0] : "kish");
        return 1;
    }

    return g.last_return_value;
}
