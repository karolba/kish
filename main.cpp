#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <cassert>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <deque>
#include "Tokenizer.h"
#include "Parser.h"
#include "Global.h"
#include "WordExpander.h"
#include "CommandExpander.h"

void set_variables(const std::vector<VariableAssignment> &variable_assignments) {
    for(const VariableAssignment &va : variable_assignments) {
        std::string value;
        if (!WordExpander(va.value).expand_into_space_joined(value)) {
            std::cerr << "Failed expansion\n";
            g.last_return_value = 1;
            return;
        }

        g.variables[va.name] = value;
    }
}

void touch_files(const std::vector<Redirection> &redirections) {
    // TODO: maybe this shouldn't be a special case? Could `>a` be equivallent in
    // results to just `{}>a`?
    for(const Redirection &redir : redirections) {
        if(redir.type != Redirection::FileWrite
            && redir.type != Redirection::FileWriteAppend
            && redir.type != Redirection::FileRead) {

            continue;
        }

        std::string path;
        if(! WordExpander(redir.path).expand_into_single_word(path)) {
            std::cerr << "shell: Ambiguous redirect\n";
            return;
        }

        if(redir.type == Redirection::FileWrite) {
            FILE *f = fopen(path.c_str(), "w");
            if(!f) perror("shell");
            if(f) fclose(f);
        }

        if(redir.type == Redirection::FileWriteAppend) {
            FILE *f = fopen(path.c_str(), "wa");
            if(!f) perror("shell");
            if(f) fclose(f);
        }

        if(redir.type == Redirection::FileRead) {
            FILE *f = fopen(path.c_str(), "r");
            if(!f) perror("shell");
            if(f) fclose(f);
        }
    }
}

template<typename T>
bool fd_open(int target_fd, const char *file, int flags, T mode) {
    int fd = open(file, flags, mode);
    if(fd == -1) {
        perror("open");
        return false;
    }
    if(fd != target_fd) {
        if(dup2(fd, target_fd) == -1) {
            perror("dup2");
            if(close(fd) == -1)
                perror("close");
            return false;
        }

        if(close(fd) == -1) {
            perror("close");
            close(target_fd);
            return false;
        }
    }
    return true;
}


// TODO: Redirection should be a variant<FileRedirection, Rewiring> so those types make sense
bool setup_file_redirection(const Redirection &redir) {
    int options;
    if(redir.type == Redirection::FileRead)
        options = O_RDONLY;
    else if(redir.type == Redirection::FileWrite)
        options = O_WRONLY | O_CREAT | O_TRUNC;
    else if(redir.type == Redirection::FileWriteAppend)
        options = O_WRONLY | O_CREAT | O_APPEND;
    else {
        fprintf(stderr, "setup_file_redirection called with a non-file redirection\n");
        return false;
    }

    return fd_open(redir.fd, redir.path.c_str(), options, 0666);
}

bool setup_rewiring(const Redirection &redir) {
    if(dup2(redir.fd, redir.rewire_fd) == -1) {
        perror("rewiring");
        return false;
    }
    return true;
}

bool setup_redirection(const Redirection &redir) {
    if(redir.type == Redirection::FileRead
            || redir.type == Redirection::FileWrite
            || redir.type == Redirection::FileWriteAppend)
        return setup_file_redirection(redir);

    if(redir.type == Redirection::Rewiring) {
        return setup_rewiring(redir);
    }

    // this should never get reached
    return false;
}

[[noreturn]]
void exec_expanded_command(const Command &expanded_command) {
    for(const Redirection &redir : expanded_command.redirections) {
        if(!setup_redirection(redir)) {
            fprintf(stderr, "kish: could not redirect\n");
            exit(1);
        }
    }

    // environment variables from `a=b c`
    for(const VariableAssignment &va : expanded_command.variable_assignments) {
        setenv(va.name.c_str(), va.value.c_str(), 1);
    }

    // TODO: ordered redirections and rewirings

    char **argv = new char*[expanded_command.argv.size() + 1];
    for(size_t i = 0; i < expanded_command.argv.size(); ++i) {
        argv[i] = strdup(expanded_command.argv.at(i).c_str());
    }
    argv[expanded_command.argv.size()] = nullptr;

    execvp(expanded_command.argv.at(0).c_str(), argv);
    perror(expanded_command.argv.at(0).c_str());
    exit(127);
}

[[noreturn]]
void expand_and_exec_command(const Command &cmd) {
    // `a=b` without any commands
    if(cmd.argv.size() == 0 && cmd.variable_assignments.size() != 0) {
        // Variable assignment sets $? to 0, command substitution can modify it further
        // Examples:
        // `true; a=$(false)`  ->  $? == 1
        // `false; a=""`       ->  $? == 0
        // That's why g.last_return_value is reset first, then any substitution is done
        g.last_return_value = 0;

        set_variables(cmd.variable_assignments);
    }

    // redirections get expanded before evaluating variable assignments in zsh,
    // but after in bash. Assuming bash-like behaviour

    // `>file` without any commands
    if(cmd.argv.size() == 0 && cmd.redirections.size() != 0) {
        touch_files(cmd.redirections);
    }


    if(cmd.argv.size() == 0) {
        exit(0);
    }

    // TODO:
    // variable expansion and command substitution should be done in the main shell process
    // EXCEPT when we are a part of a multi-command pipeline, then every subcommand happens in a subshell
    Command expanded = cmd;
    if(!CommandExpander(&expanded).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        exit(1);
    }

    exec_expanded_command(expanded); // noreturn
}

std::optional<pid_t> run_command_expand_in_main_process(const Command &cmd) {
    // `a=b` without any commands
    if(cmd.argv.size() == 0 && cmd.variable_assignments.size() != 0) {
        // Variable assignment sets $? to 0, command substitution can modify it further
        // Examples:
        // `true; a=$(false)`  ->  $? == 1
        // `false; a=""`       ->  $? == 0
        // That's why g.last_return_value is reset first, then any substitution is done
        g.last_return_value = 0;

        set_variables(cmd.variable_assignments);
    }

    // redirections get expanded before evaluating variable assignments in zsh,
    // but after in bash. Assuming bash-like behaviour

    // `>file` without any commands
    if(cmd.argv.size() == 0 && cmd.redirections.size() != 0) {
        touch_files(cmd.redirections);
    }

    if(cmd.argv.size() == 0) {
        return {};
    }

    Command expanded = cmd;

    if(!CommandExpander(&expanded).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        g.last_return_value = 1;
        return {};
    }

    int pid = fork();
    if(pid == -1) {
        perror("fork");
        return {};
    }
    if(pid == 0) {
        // child
        exec_expanded_command(expanded); // noreturn
    }
    return { pid };
}

std::optional<pid_t> run_command_expand_in_subprocess(const Command &cmd) {
    int pid = fork();
    if(pid == -1) {
        perror("fork");
        return {};
    }
    if(pid == 0) {
        expand_and_exec_command(cmd); // noreturn
    }
    return { pid };
}

void wait_for_one(pid_t pid) {
    int status;

    do {
        if(waitpid(pid, &status, WUNTRACED | WCONTINUED) == -1) {
            // TODO: do we get here only when interrupted?
            perror("waitpid");
            exit(1);
        }

//        if (WIFEXITED(status)) {
//            printf("child exited, status=%d\n", WEXITSTATUS(status));
//        } else if (WIFSIGNALED(status)) {
//            printf("child killed (signal %d)\n", WTERMSIG(status));
//        } else if (WIFSTOPPED(status)) {
//            printf("child stopped (signal %d)\n", WSTOPSIG(status));
//        } else if (WIFCONTINUED(status)) {
//            printf("child continued\n");
//        } else {    /* Non-standard case -- may never happen */
//            printf("Unexpected status (0x%x)\n", status);
//        }
    } while(!WIFEXITED(status) && !WIFSIGNALED(status));

    if(WIFEXITED(status))
        g.last_return_value = WEXITSTATUS(status);
}

void wait_for_all(const std::vector<int> &pids) {
    for(pid_t pid : pids) {
        wait_for_one(pid);
    }
}

void setup_pipe_between(Command &left, Command &right) {
    // TODO
}

void run_multi_command_pipeline(const Pipeline &pipeline) {
    Pipeline modified = pipeline;
    for(size_t i = 1; i < modified.size(); ++i) {
        setup_pipe_between(modified[i - 1], modified[i]);
    }

    std::vector<pid_t> pids;
    for(const Command &cmd : modified) {
        auto maybe_pid = run_command_expand_in_subprocess(cmd);
        if(maybe_pid)
            pids.push_back(maybe_pid.value());

        // TODO: implement actually piping data
    }

    wait_for_all(pids);
}

void run_single_command_pipeline(const Command &command) {
    auto pid = run_command_expand_in_main_process(command);

    if(pid)
        wait_for_one(pid.value());
}

void run_pipeline(const Pipeline &pipeline) {
    if(pipeline.size() == 1) {
        run_single_command_pipeline(pipeline.at(0));
    } else if(pipeline.size() > 1) {
        run_multi_command_pipeline(pipeline);
    }
}

void run_and_or_list(const AndOrList &and_or_list) {
    for(const WithFollowingOperator<Pipeline> &pipe_op : and_or_list) {
        run_pipeline(pipe_op.val);

        if(pipe_op.following_operator == "&&" && g.last_return_value != 0)
            return;
        if(pipe_op.following_operator == "||" && g.last_return_value == 0)
            return;
    }
}

void run_command_list(const CommandList &cl) {
    for(const WithFollowingOperator<AndOrList> &aol_op : cl) {
        run_and_or_list(aol_op.val);
        // aol_and_op.following_operator is ";" or "" or "&" (TODO)
    }
}

void run_from_string(const std::string &str) {
    std::vector<Token> tokens = Tokenizer(str).tokenize();
    CommandList parsed = Parser(tokens).parse();

    run_command_list(parsed);
}

int main(int argc, char *argv[]) {

    if(argc == 3 && strcmp(argv[1], "-c") == 0) {
        run_from_string(argv[1]);
    } else {
        std::string input_line;
        while(std::cout << "$ " << std::flush && std::getline(std::cin, input_line)) {
            run_from_string(input_line);
        }
    }
}
