#include "echo.h"
#include "../Parser.h"

#include <iostream>

int builtin_echo(const Command::Simple &cmd) {
    std::size_t start = 1;
    bool endWithNewline = true;

    if(cmd.argv.size() >= 2 && cmd.argv.at(1) == "-n") {
        endWithNewline = false;
        start += 1;
    }

    for(std::size_t i = start; i < cmd.argv.size(); i++) {
        if (i != start)
            std::cout << ' ';
        std::cout << cmd.argv.at(i);
    }
    if(endWithNewline)
        std::cout << "\n";

    return 0;
}
