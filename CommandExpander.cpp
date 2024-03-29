#include "CommandExpander.h"
#include "WordExpander.h"
#include <stdio.h>
#include <variant>

bool CommandExpander::expand()
{
    if(std::holds_alternative<Command::Simple>(m_command->value)) {
        // values of assigned variables should always be space-joined
        // so that when $IFS starts with ' ', `var="$@"` is to be equivalent to `var="$*"`
        // and `var=$(echo a b)` equivalent to `var='a b'`
        auto &simple_command = std::get<Command::Simple>(m_command->value);
        for (auto& assignment : simple_command.variable_assignments) {
            std::string expanded;

            WordExpander::Options opt;
            opt.commonExpansions = true;
            opt.fieldSplitting = false;
            opt.pathnameExpansion = WordExpander::Options::NEVER;
            opt.variableAtAsMultipleFields = false;
            if(! WordExpander(opt, assignment.value).expand_into(expanded)) {
                word_expansion_failed(assignment.value);
                return false;
            }

            assignment.value = expanded;
        }
    }

    for (auto& redirection : m_command->redirections) {
        if(redirection.type != Redirection::Rewiring) {
            std::vector<std::string> expanded;

            WordExpander::Options opt;
            opt.commonExpansions = true;
            opt.fieldSplitting = false;
            opt.pathnameExpansion = WordExpander::Options::ONLY_IF_SINGLE_RESULT;
            opt.variableAtAsMultipleFields = false;
            if(! WordExpander(opt, redirection.path).expand_into(expanded)) {
                word_expansion_failed(redirection.path);
                return false;
            }
            if (expanded.size() != 1) {
                fprintf(stderr, "Shell: %s: ambiguous redirect\n", redirection.path.c_str());
                return false;
            }
            redirection.path = std::move(expanded[0]);
        }
    }


    if(std::holds_alternative<Command::Simple>(m_command->value)) {
        auto &simple_command = std::get<Command::Simple>(m_command->value);

        std::vector<std::string> unexpanded_argv = simple_command.argv;
        simple_command.argv.clear();
        for (const std::string& arg : unexpanded_argv) {
            WordExpander::Options opt;
            opt.commonExpansions = true;
            opt.fieldSplitting = true;
            opt.pathnameExpansion = WordExpander::Options::ALWAYS;
            opt.variableAtAsMultipleFields = true;
            if (!WordExpander(opt, arg).expand_into(simple_command.argv)) {
                fprintf(stderr, "Shell: %s: failed expanding word\n", arg.c_str());
            }
        }
    }

    return true;
}

void CommandExpander::word_expansion_failed(const std::string &word)
{
    fprintf(stderr, "Shell: %s: word expansion failed\n", word.c_str());
}

