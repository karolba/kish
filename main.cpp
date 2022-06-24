#include <string.h>
#include "executor.h"
#include <string>
#include <iostream>
#include <unistd.h>
#include "Global.h"

void initialize_variables() {
    // "In a subshell, '$' shall expand to the same value as that of the current shell."
    g.variables["$"] = std::to_string(getpid());
}

static bool echo_prompt() {
    char *wd = getwd(NULL);
    if(wd == nullptr)
        return false;

    bool successfull = !!(std::cout << wd << " $ " << std::flush);
    free(wd);
    return successfull;
}

int main(int argc, char *argv[]) {
    initialize_variables();

    if(argc == 3 && strcmp(argv[1], "-c") == 0) {
        run_from_string(argv[2]);
    } else {
        std::string input_line;
        while(echo_prompt() && std::getline(std::cin, input_line)) {
            run_from_string(input_line);
        }
    }

    return g.last_return_value;
}
