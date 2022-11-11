#include "utils.h"
#include <unistd.h>
#include <sys/wait.h>
#include "Global.h"

namespace utils {

void wait_for_one(pid_t pid) {
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

void wait_for_all(const std::vector<int> &pids) {
    for(pid_t pid : pids) {
        wait_for_one(pid);
    }
}

} // namespace utils
