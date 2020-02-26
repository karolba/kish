#include "CommandExpander.h"
#include "WordExpander.h"
#include <stdio.h>

//#include "nodbg.cpp"

bool CommandExpander::expand()
{
    // values of assigned variables should always be space-joined
    // so that `var="$@"` is to be equivalent to `var="$*"`
    // and `var=$(echo a b)` equivalent to `var='a b'`
    for (auto& assignment : m_command->variable_assignments) {
        std::vector<std::string> expanded;
        if(! WordExpander(assignment.value).expand_into(expanded)) {
            word_expansion_failed(assignment.value);
            return false;
        }

        std::string builder;
        for (size_t i = 0; i < expanded.size(); ++i) {
            if (i != 0) {
                builder.push_back(' ');
            }
            builder += expanded[i];
        }
        assignment.value = builder;
        setenv(assignment.name.c_str(), assignment.value.c_str(), 1);
        //dbg() << expanded[0] << '\n';
    }

    // behave like bash, make `> $empty_var` and `> $(echo 1 2)` illegal
    for (auto& redirection : m_command->redirections) {
        if(redirection.type != Redirection::Rewiring) {
            std::vector<std::string> expanded;
            if(! WordExpander(redirection.path).expand_into(expanded)) {
                word_expansion_failed(redirection.path);
                return false;
            }
            if (expanded.size() != 1) {
                fprintf(stderr, "Shell: %s: ambiguous redirect\n", redirection.path.c_str());
                return false;
            }
            redirection.path = move(expanded[0]);
        }
    }

    std::vector<std::string> unexpanded_argv = m_command->argv;

    m_command->argv.clear();
    for (const std::string& arg : unexpanded_argv) {
        if (!WordExpander(arg).expand_into(m_command->argv)) {
            fprintf(stderr, "Shell: %s: failed expanding word\n", arg.c_str());
        }
    }

    //dbgprintf("argv after expansion (not used):\n\t");
    //for (const auto& word : m_command->argv) {
    //    dbgprintf("<%s> ", word.characters());
    //}
    //dbgprintf("\n");
    

    return true;
}

void CommandExpander::word_expansion_failed(const std::string &word)
{
    fprintf(stderr, "Shell: %s: word expansion failed\n", word.c_str());
}

