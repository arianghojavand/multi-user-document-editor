//TODO: client code that can send instructions to server.

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <stdio.h>

volatile sig_atomic_t got_signal = 0;

void sig_handler(int signal) {
    (void)signal;
    got_signal = 1;
}

// void setup_signals(void) {

//     struct sigaction sa = {
//         .sa_flags = 0,
//         .sa_handler = sig_handler,
//     };

//     sigemptyset(&sa.sa_mask);
//     sigaction(SIGUSR1, &sa, NULL);
// }

void wait_for_signal(void) {

}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        puts("client: invalid num args");
    }

    pid_t server_pid;
    char* username;

    sscanf(argv[1], "%d", &server_pid);
    username = argv[2];
    
    printf("%d and %s\n", server_pid, username);
    
    return 0;

}