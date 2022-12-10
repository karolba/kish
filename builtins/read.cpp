#include "read.h"
#include <string>
#include <iostream>
#include "../Global.h"
#include "../utils.h"

int builtin_read(const Command::Simple &cmd)
{
    // TODO: this can't be an std::vector<std::string_view> due to g.variables
    //       (std::unordered_map<std::string, std::string>) not being able to
    //       be queried from a string view
    std::vector<std::string> variables;

    if(cmd.argv.size() <= 1) {
        // default variable name for read(1)
        variables = { "REPLY" };
    } else {
        bool skippedFirst = false;

        for(const std::string &str : cmd.argv) {
            // skip argv[0] == "read"
            if(!skippedFirst) {
                skippedFirst = true;
                continue;
            }

            if(str.empty()) {
                // empty variable name?
                // let's just ignore that
                continue;
            }

            if(str.at(0) == '-') {
                // TODO: actually parse options
                // propably not really relevant for most use cases
                continue;
            }

            variables.push_back(str);
        }
    }

    std::string line;
    if(! std::getline(std::cin, line)) {
        return 1;
    }

    if(variables.size() == 1) {
        g.variables[variables.at(0)] = line;
    } else {
        utils::Splitter splitter(line);

        std::size_t partNumber = 0;
        splitter.delimIFS().for_each([&] (const std::string &part) -> utils::Splitter::ShouldContinue {
            if(partNumber >= variables.size() - 1)
                return utils::Splitter::BREAK_LOOP;

            g.variables[variables.at(partNumber)] = part;

            partNumber += 1;
            return utils::Splitter::CONTINUE_LOOP;
        });
        g.variables[variables.at(variables.size() - 1)] = splitter.rest_of_input();
    }


    return 0;
}
