#include "job_control.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "Global.h"

namespace job_control {

static pid_t shell_pgid;
static struct termios shell_tmodes;
static int shell_terminal;
static bool shell_is_interactive = false;

// This file is based on
// https://www.gnu.org/software/libc/manual/html_node/Implementing-a-Shell.html

void init_interactive_shell() {
    /* See if we are running interactively.  */
    shell_terminal = STDOUT_FILENO;
    shell_is_interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && isatty(STDERR_FILENO);

    if (shell_is_interactive) {
        /* Loop until we are in the foreground.  */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        /* Ignore interactive and job-control signals.  */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        /* Put ourselves in our own process group.  */
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

void noninteractive_wait_for_one(pid_t pid) {
    int status;

    do {
        if(waitpid(pid, &status, WUNTRACED | WCONTINUED) == -1) {
            // TODO: do we get here only when interrupted?
            perror("waitpid");
            exit(1);
        }
    } while(!WIFEXITED(status) && !WIFSIGNALED(status));

    if(WIFEXITED(status))
        g.last_return_value = WEXITSTATUS(status);
}

void wait_for_one(pid_t pid) {
    if(shell_is_interactive) {
        /* Put the job into the foreground.  */
        tcsetpgrp(job_control::shell_terminal, pid);
    }

    noninteractive_wait_for_one(pid);

    if(shell_is_interactive) {
        /* Put the shell back in the foreground.  */
        tcsetpgrp(job_control::shell_terminal, job_control::shell_pgid);

        /* Restore the shell’s terminal modes.  */
        //tcgetattr(job_control::shell_terminal, job_control::shell_tmodes);
        tcsetattr(job_control::shell_terminal, TCSADRAIN, &job_control::shell_tmodes);
    }
}

void wait_for_all(const std::vector<int> &pids) {
    for(pid_t pid : pids) {
        wait_for_one(pid);
    }
}

void before_exec_no_pipeline(bool foreground) {
    if (shell_is_interactive) {
        /* give the process group the terminal, if appropriate. */

        // TODO: for pipelines, don't assume pgid == pid
        pid_t pgid = getpgid(0);
        if (foreground)
            tcsetpgrp(shell_terminal, pgid);

        /* Set the handling for job control signals back to the default.  */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
    }
}

pid_t fork_own_process_group() {
    pid_t pid = fork();
    if(shell_is_interactive && pid >= 0) {
        // Put the process into the process group
        // This has to be done both by the shell and in the individual
        // child processes because of potential race conditions.
        setpgid(pid, 0);
    }
    return pid;
}

} // namespace job_control
