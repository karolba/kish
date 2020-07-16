#include "read.h"
#include <string>
#include <iostream>
#include "../Global.h"

int builtin_read(const Command::Simple &cmd)
{
    std::string line;

    if(! std::getline(std::cin, line)) {
        return 1;
    }

    std::string name = cmd.argv[1];
    g.variables[name] = line;

    return 0;
}
