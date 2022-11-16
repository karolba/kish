#include "source.h"

#include <iostream>
#include <fstream>
#include "../executor.h"

int builtin_source(const Command::Simple &expanded_command) {
    if(expanded_command.argv.size() != 2) {
        std::cerr << "Usage: source <file-path>\n";
        return 1;
    }

    std::string file_path = expanded_command.argv.at(1);

    std::string lines;
    if(std::ifstream f{file_path}) {
        // TODO: make this efficiant
        // TODO: don't save the whole file at all
        std::string line;
        while(std::getline(f, line)) {
            lines.append(line);
            lines.append("\n");
        }
    } else {
        std::cerr << "source: Cannot access file '" << file_path << "'\n";
        return 1;
    }

    executor::run_from_string(lines);
    return 0;
}
