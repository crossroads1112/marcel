#define _XOPEN_SOURCE

#include <stdio.h> // puts
#include <signal.h> // SIGINT, SIGCHLD, SIG_IGN, sigaction...

#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid

#include "children.h" // del_bkg_proc, reset_active_child
#include "macros.h" // Arr_len
#include "marcel.h" // exit_code
#include "signals.h" // siglongjmp

int signals[] = { SIGINT, SIGCHLD, SIGQUIT };
sigjmp_buf _sigbuf;

static void handle_signals(int signo)
{
    pid_t p = 0;
    _Bool first_iter = 1;
    int status;

    switch (signo) {
    case SIGINT:
        exit_code = SIGINT_EXIT_CODE;
        puts("");
        siglongjmp(_sigbuf, 1);
        break;
    case SIGQUIT:
        puts("\nQuit (core dumped)");
        // Ignored signals are inherited by child. By putting SIG_IGN in a
        // signal handling function, child will still respond to SIGINT
        SIG_IGN;
        break;
    case SIGCHLD:
        for (;;) {
            p = waitpid(-1, &status, WNOHANG | WUNTRACED);
            if (p == 0) {
                // Active child generates SIGCHLD but background process 
                // still running
                break;
            } else if (p == -1) {
                // No child processes. This could occur because SIGCHLD was 
                // generated by active child after cleanup code ran or this loop
                // has cleaned up all background processes. If this is the first
                // iteration of this loop, it could only be the former.
                if (first_iter) reset_active_child();
                break;
            } else if (p == get_active_child()) {
                // SIGCHLD generated before active child could be cleaned up
                reset_active_child();
                first_iter = 0;
                continue;
            }

            size_t job_num = del_bkg_proc(p);
            printf("[%zu] completed. Exit: %d\n", job_num, WEXITSTATUS(status));
            first_iter = 0;
        }
        break;
    }

}


int setup_signals(void)
{
    struct sigaction act = {0};
    if (sigemptyset(&act.sa_mask) == -1) return 1;
    act.sa_handler = &handle_signals;

    size_t len = Arr_len(signals);
    for (size_t i = 0; i < len; i++) {
        if (sigaction(signals[i], &act, NULL) == -1) return 1;
    }

    return 0;
}
