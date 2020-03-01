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

} // namespace utils
