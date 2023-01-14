#pragma once

#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace job_control {

/* based on the GNU libc manual */
/* so the API of this file is kind of C-like */
void wait_for_one(pid_t pid);
void wait_for_all(const std::vector<int> &pids);
void init_interactive_shell();
void before_exec_no_pipeline(bool foreground);
pid_t fork_own_process_group();

}
