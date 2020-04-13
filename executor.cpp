#include "executor.h"
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
#include <functional>
#include "builtins.h"
#include <variant>
#include "utils.h"


static void run_command_list(const CommandList &cl);

static void set_variables(const std::vector<VariableAssignment> &variable_assignments) {
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

static bool touch_file(const std::string &path, const char *mode) {
    FILE *f = fopen(path.c_str(), mode);
    if(!f) {
        perror("shell");
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

static bool touch_files(const std::vector<Redirection> &redirections) {
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
            return false;
        }

        if(redir.type == Redirection::FileWrite) {
            if(!touch_file(path, "w"))
                return false;
        } else if(redir.type == Redirection::FileWriteAppend) {
            if(!touch_file(path, "wa"))
                return false;
        } else if(redir.type == Redirection::FileRead) {
            if(!touch_file(path, "r"))
                return false;
        }
    }

    return true;
}

template<typename T>
static bool fd_open(int target_fd, const char *file, int flags, T mode) {
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

static int get_unused_fd() {
    // TODO: should saved_fd.original_fd also be counted?
    int fd = 10;

    auto saved_fds = g.saved_fds;
    std::sort(saved_fds.begin(), saved_fds.end(), [](const auto &s1, const auto &s2) {
        return s1.saved_location < s2.saved_location;
    });

    for(Global::SavedFd saved : saved_fds) {
        if(saved.saved_location == fd)
            fd++;
    }

    return fd;
}

static std::optional<int> fd_save(int original_fd) {
    int new_fd = get_unused_fd();
    if(dup2(original_fd, new_fd) == -1) {
        perror("dup2");
        return {};
    }
    // No need to close(original_fd) because dup2 closes the dest fd

    g.saved_fds.push_back(Global::SavedFd{original_fd, new_fd});
    return { new_fd };
}

static void fd_restore(int saved_fd) {
    int original_location = -1;

    // Find the *last* saved file descriptor
    for(auto saved : g.saved_fds) {
        if(saved.saved_location == saved_fd)
            original_location = saved.original_fd;
    }

    g.saved_fds.erase(std::remove_if(
          g.saved_fds.begin(),
          g.saved_fds.end(),
          [&] (Global::SavedFd s) { return s.saved_location == saved_fd && s.original_fd == original_location; }),
       g.saved_fds.end());

    if(dup2(saved_fd, original_location) == -1) {
        perror("dup2");
    }

    close(saved_fd);
}

static int file_redirection_type_to_open_option(const Redirection &redir) {
    if(redir.type == Redirection::FileRead)
        return O_RDONLY;
    else if(redir.type == Redirection::FileWrite)
        return O_WRONLY | O_CREAT | O_TRUNC;
    else if(redir.type == Redirection::FileWriteAppend)
        return O_WRONLY | O_CREAT | O_APPEND;
    else {
        fprintf(stderr, "kish: file_redirection_type_to_open_option called with a non-file redirection\n");
        exit(1);
    }
}

// TODO: Redirection should be a variant<FileRedirection, Rewiring> so those types make sense
static bool setup_file_redirection(const Redirection &redir) {
    int options = file_redirection_type_to_open_option(redir);

    return fd_open(redir.fd, redir.path.c_str(), options, 0666);
}

static bool setup_rewiring(const Redirection &redir) {
    if(redir.fd == redir.rewire_fd)
        return true;

    if(dup2(redir.rewire_fd, redir.fd) == -1) {
        perror("rewiring");
        return false;
    }

    return true;
}

static bool setup_redirection(const Redirection &redir) {
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

/*
 * Used to save (and later restore, via restore_old_fds()) fds when executing, among others:
 * - redirections applied to builtins: `echo $var > file`
 * - redirections to non-pipelined command lists: `{ a; b; } > file`
 */
static std::deque<int> setup_redirections_save_old_fds(const std::vector<Redirection> &redirs) {
    // The naive approach to handle this would be
    // - save the old fd
    // - open() and move the resulting fd to the old fd
    //
    // But redirection cannot be done that way. Consider the following:
    //        { ...; } > /dev/stdout
    //    ==  { ...; } > /proc/self/fd/1
    // In this case the naive method would fail, because stdout (fd=1) would already be moved to a different fd to be replaced,
    // so when open() tries to access it, there's nothing in fd 1.
    //
    // To prevent this,
    // - open() needs to be called first,
    // - then the original fd needs to be saved,
    // - and then the resulting fd from open() needs to be moved to where the original fd was

    std::deque<int> saved_fds;

    for(const Redirection &redir : redirs) {

        if(redir.type == Redirection::Type::Rewiring) { // (for example, 2>&3)
            if(redir.fd == redir.rewire_fd)
                continue;

            auto saved_fd = fd_save(redir.fd);
            if(!saved_fd)
                continue;

            saved_fds.push_front(saved_fd.value());

            if(dup2(redir.rewire_fd, redir.fd) == -1) {
                perror("dup2");
                continue;
            }
        } else { // file redirection (>file, >>file, ..)
            int new_fd = open(redir.path.c_str(), file_redirection_type_to_open_option(redir), 0666);
            if(new_fd == -1) {
                perror("open");
                continue;
            }

            auto saved_fd = fd_save(redir.fd);
            if(!saved_fd)
                continue;

            saved_fds.push_front(saved_fd.value());

            if(dup2(new_fd, redir.fd) == -1) {
                perror("dup2");
                continue;
            }

            if(close(new_fd) == -1) {
                perror("close");
                continue;
            }
        }
    }

    return saved_fds;
}

static void restore_old_fds(const std::deque<int> &old_fds) {
    for(int saved_fd : old_fds) {
        fd_restore(saved_fd);
    }
}

[[noreturn]]
static void exec_expanded_simple_command(const Command &expanded_command, const bool search_for_builitin) {
    const Command::Simple &expanded_simple = std::get<Command::Simple>(expanded_command.value);

    for(const Redirection &redir : expanded_command.redirections) {
        if(!setup_redirection(redir)) {
            fprintf(stderr, "kish: could not redirect\n");
            exit(1);
        }
    }

    // environment variables from `a=b c`
    for(const VariableAssignment &va : expanded_simple.variable_assignments) {
        setenv(va.name.c_str(), va.value.c_str(), 1);
    }

    if(search_for_builitin) {
        auto builtin = find_builtin(expanded_simple.argv.at(0));
        if(builtin) {
            int error_code = builtin.value()(expanded_simple);
            exit(error_code);
        }
    }

    char **argv = new char*[expanded_simple.argv.size() + 1];
    for(size_t i = 0; i < expanded_simple.argv.size(); ++i) {
        argv[i] = strdup(expanded_simple.argv.at(i).c_str());
    }
    argv[expanded_simple.argv.size()] = nullptr;

    execvp(expanded_simple.argv.at(0).c_str(), argv);
    perror(expanded_simple.argv.at(0).c_str());
    exit(127);
}

[[noreturn]]
static void expand_and_exec_empty_command(const Command &cmd) {
    if(touch_files(cmd.redirections))
        exit(0);
    else
        exit(1);
}

[[noreturn]]
static void expand_and_exec_simple_command(Command cmd) {
    const Command::Simple &simple_command = std::get<Command::Simple>(cmd.value);

    // `a=b` without any commands
    if(simple_command.argv.size() == 0 && simple_command.variable_assignments.size() != 0) {
        // Variable assignment sets $? to 0, command substitution can modify it further
        // Examples:
        // `true; a=$(false)`  ->  $? == 1
        // `false; a=""`       ->  $? == 0
        // That's why g.last_return_value is reset first, then any substitution is done
        g.last_return_value = 0;

        set_variables(simple_command.variable_assignments);
    }

    // redirections get expanded before evaluating variable assignments in zsh,
    // but after in bash. Assuming bash-like behaviour

    // `>file` without any commands
    if(simple_command.argv.size() == 0 && cmd.redirections.size() != 0) {
        if(!touch_files(cmd.redirections))
            exit(1);
    }


    if(simple_command.argv.size() == 0) {
        exit(0);
    }

    // when we are a part of a multi-command pipeline, every substitution happens in a subshell
    if(!CommandExpander(&cmd).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        exit(1);
    }

    exec_expanded_simple_command(cmd, true);
}

[[noreturn]]
static void expand_and_exec_compound_command(Command expanded) {
    // expand redirections:
    if(!CommandExpander(&expanded).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        exit(1);
    }
    for(const Redirection &redir : expanded.redirections) {
        if(!setup_redirection(redir)) {
            fprintf(stderr, "kish: could not redirect\n");
            exit(1);
        }
    }
    const Command::Compound &compound = std::get<Command::Compound>(expanded.value);
    run_command_list(compound.command_list);
    exit(g.last_return_value);
}

[[noreturn]]
static void expand_and_exec_if_command(Command cmd) {
    if(!CommandExpander(&cmd).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        exit(1);
    }
    for(const Redirection &redir : cmd.redirections) {
        if(!setup_redirection(redir)) {
            fprintf(stderr, "kish: could not redirect\n");
            exit(1);
        }
    }
    g.last_return_value = 0;
    const Command::If &if_command = std::get<Command::If>(cmd.value);
    run_command_list(if_command.condition);
    if(g.last_return_value == 0)
        run_command_list(if_command.then);
    exit(g.last_return_value);
}

// Runs a pipelined command
static std::optional<pid_t> run_command_expand_in_subprocess(const Command &cmd) {
    int pid = fork();
    if(pid == -1) {
        perror("fork");
        return {};
    }
    if(pid == 0) {
        std::visit(utils::overloaded {
              [&] (const Command::Empty) { expand_and_exec_empty_command(cmd); },
              [&] (const Command::Simple) { expand_and_exec_simple_command(cmd); },
              [&] (const Command::Compound) { expand_and_exec_compound_command(cmd); },
              [&] (const Command::If) { expand_and_exec_if_command(cmd); },
        }, cmd.value);
    }
    return { pid };
}

// Runs a non-pipelined builtin with possible redirections
static void run_builitin_in_main_process(BuiltinHandler &builtin, const Command &expanded_command) {
    const auto &simple_command = std::get<Command::Simple>(expanded_command.value);

    auto old_fds = setup_redirections_save_old_fds(expanded_command.redirections);
    g.last_return_value = builtin(simple_command);
    restore_old_fds(old_fds);
}

// Runs non-pipelined commands composed of only redirections
static void run_empty_command_expand_in_main_process(const Command &cmd) {
    // `>file` with no commands
    if(!touch_files(cmd.redirections))
        g.last_return_value = 1;
}

// Runs non-pipelined commands composed of only variable assignments with optional redirecions
static void run_empty_simple_command_expand_in_main_process(const Command &cmd) {
    const Command::Simple &simple_command = std::get<Command::Simple>(cmd.value);

    assert(simple_command.argv.size() == 0);

    if(simple_command.variable_assignments.size() != 0) {
        // Variable assignment sets $? to 0, command substitution can modify it further
        // Examples:
        // `true; a=$(false)`  ->  $? == 1
        // `false; a=""`       ->  $? == 0
        // That's why g.last_return_value is reset first, then any substitution is done
        g.last_return_value = 0;

        set_variables(simple_command.variable_assignments);
    }

    // redirections get expanded before evaluating variable assignments in zsh,
    // but after in bash. Assuming bash-like behaviour

    // `>file` with no commands
    if(cmd.redirections.size() != 0) {
        if(!touch_files(cmd.redirections))
            g.last_return_value = 1;
    }
}

// Runs non-pipelined simple commands (e.g. `a=b c d >e`) that have argv in them
static void run_nonempty_simple_command_expand_in_main_process(Command expanded) {
    if(!CommandExpander(&expanded).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        g.last_return_value = 1;
        return;
    }

    const Command::Simple &expanded_simple_command = std::get<Command::Simple>(expanded.value);

    auto builtin = find_builtin(expanded_simple_command.argv.at(0));
    if(builtin) {
        run_builitin_in_main_process(builtin.value(), expanded);
    } else {
        int pid = fork();
        if(pid == -1) {
            perror("fork");
            return;
        }
        if(pid == 0) {
            // child
            exec_expanded_simple_command(expanded, false); // noreturn
        }
        utils::wait_for_one(pid);
   }

}

// Runs non-pipelined simple commands (e.g. `a=b c d >e`)
static void run_simple_command_expand_in_main_process(const Command &cmd) {
    const Command::Simple &simple_command = std::get<Command::Simple>(cmd.value);

    if(simple_command.argv.size() == 0)
        run_empty_simple_command_expand_in_main_process(cmd);
    else
        run_nonempty_simple_command_expand_in_main_process(cmd);
}

// Runs non-pipelined compound commands (e.g `{ a; b; } > c`)
static void run_compound_command_expand_in_main_process(Command cmd) {
    if(!CommandExpander(&cmd).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        g.last_return_value = 1;
        return;
    }

    const Command::Compound &compound_command = std::get<Command::Compound>(cmd.value);

    auto saved_fds = setup_redirections_save_old_fds(cmd.redirections);

    run_command_list(compound_command.command_list);

    restore_old_fds(saved_fds);
}

static void run_if_command_expand_in_main_process(Command cmd) {
    if(!CommandExpander(&cmd).expand()) {
        fprintf(stderr, "Command expansion failed\n");
        g.last_return_value = 1;
        return;
    }

    const Command::If &if_command = std::get<Command::If>(cmd.value);

    auto saved_fds = setup_redirections_save_old_fds(cmd.redirections);

    g.last_return_value = 0;
    run_command_list(if_command.condition);
    if(g.last_return_value == 0) {
        run_command_list(if_command.then);
    }

    restore_old_fds(saved_fds);
}

// Runs any non-pipelined command
static void run_command_expand_in_main_process(const Command &cmd) {
    std::visit(utils::overloaded {
          [&] (const Command::Empty) { run_empty_command_expand_in_main_process(cmd); },
          [&] (const Command::Simple) { run_simple_command_expand_in_main_process(cmd); },
          [&] (const Command::Compound) { run_compound_command_expand_in_main_process(cmd); },
          [&] (const Command::If) { run_if_command_expand_in_main_process(cmd); },
    }, cmd.value);
}

// Setup pipe between two commands: `left | right`
static void setup_pipe_between(Command &left, Command &right) {
    int pipefd[2];
    if(pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }
    left.redirections.push_back(Redirection{Redirection::Rewiring, STDOUT_FILENO, pipefd[1]});
    right.redirections.push_back(Redirection{Redirection::Rewiring, STDIN_FILENO, pipefd[0]});

    left.pipe_file_descriptors.push_back(pipefd[1]);
    right.pipe_file_descriptors.push_back(pipefd[0]);
}

// Runs a multi-command pipeline, for example: `a | b | c`
static void run_multi_command_pipeline(const Pipeline &pipeline) {
    std::vector<int> fds;
    Pipeline modified = pipeline;

    std::vector<pid_t> pids;
    for(size_t i = 0; i < modified.size(); ++i) {
        Command &cmd = modified[i];
        if(i != modified.size() - 1) {
            Command &next = modified[i + 1];
            setup_pipe_between(cmd, next);
        }

        auto maybe_pid = run_command_expand_in_subprocess(cmd);

        for(int fd : cmd.pipe_file_descriptors)
            close(fd); // ignore errors

        if(maybe_pid)
            pids.push_back(maybe_pid.value());
    }

    utils::wait_for_all(pids);
}

static void run_single_command_pipeline(const Command &command) {
    run_command_expand_in_main_process(command);
}

static void run_pipeline(const Pipeline &pipeline) {
    if(pipeline.size() == 1) {
        run_single_command_pipeline(pipeline.at(0));
    } else if(pipeline.size() > 1) {
        run_multi_command_pipeline(pipeline);
    }
}

static void run_and_or_list(const AndOrList &and_or_list) {
    for(const WithFollowingOperator<Pipeline> &pipe_op : and_or_list) {
        run_pipeline(pipe_op.val);

        if(pipe_op.following_operator == "&&" && g.last_return_value != 0)
            return;
        if(pipe_op.following_operator == "||" && g.last_return_value == 0)
            return;
    }
}

static void run_command_list(const CommandList &cl) {
    for(const WithFollowingOperator<AndOrList> &aol_op : cl) {
        run_and_or_list(aol_op.val);
        // aol_and_op.following_operator is ";" or "" or "&" (TODO)
    }
}

void run_from_string(const std::string &str) {
    std::vector<Token> tokens = Tokenizer(str).tokenize();
    std::optional<CommandList> parsed;

    try {
        parsed = Parser(tokens).parse();
    } catch(const Parser::SyntaxError &se) {
        std::cerr << "Parser syntax error: " << se.explanation << "\n";
        return;
    }

    run_command_list(*parsed);
}
