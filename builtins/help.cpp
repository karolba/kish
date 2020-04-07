#include "help.h"
#include "../Parser.h"
#include <stdio.h>

int builtin_help(const Command::Simple &) {
    fprintf(stdout, "This is some help\n");
    return 0;
}
