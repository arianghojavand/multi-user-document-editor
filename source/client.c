//TODO: client code that can send instructions to server.
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

// volatile sig_atomic_t got_signal = 0;

void setup_handler(sigset_t* mask);

int main(int argc, char* argv[]) {
    //(1) check number of arguments is correct
    if (argc != 3) {
        puts("client: invalid num args");
        exit(1);
    }

    //(2) get relevant details from commmand line
    pid_t server_pid;
    char* username;

        //(2.1) get server_pid
    if (sscanf(argv[1], "%d", &server_pid) != 1) {
        fprintf(stderr, "error in server_pid argument\n");
        exit(1);
    }
    
        //(2.2) get username
    username = argv[2];
    printf("%d and %s\n", server_pid, username);

    //(3) setup signal handling for SIGRTMIN
    sigset_t mask;
    setup_handler(&mask);

    printf("client pid is: %d\n", getpid());

    //(4) send SIGRTMIN to server from client
    if (kill(server_pid, SIGRTMIN) == -1) {
        perror("Error sending signal");
        exit(1);
    }

    //(5) wait for server and if successful handle signal back

    int signal_received;
    if (sigwait(&mask, &signal_received) == 0 && signal_received == SIGRTMIN + 1) {
        //good to go
        //printf("Client: received signal from server.\n");

        int c_read, c_write;

        char c2s[50], s2c[50];

        snprintf(c2s, sizeof(c2s), "FIFO_C2S_%d", getpid());
        snprintf(s2c, sizeof(s2c), "FIFO_S2C_%d", getpid());

        if ((c_write = open(c2s, O_WRONLY)) == -1) {
            fprintf(stderr, "Client: error making c2s pipe.\n");
            exit(1);
        }

        if ((c_read = open(s2c, O_RDONLY)) == -1){
            fprintf(stderr, "Client: error making s2c pipe.\n");
            close(c_write);
            exit(1);
        }

        //write username to server;
        write(c_write, username, strlen(username) + 1);

        //buffer to read response from server
        char buffer[2048];
        read(c_read, buffer, sizeof(buffer));
        printf("My perm is: %s\n", buffer);

        if (strcmp(buffer, "Reject UNAUTHORISED.\n") == 0) {
            printf("Client: server rejected me.\n");
            
        } else {

            //let the client edit

            char command[256]; 
            while (fgets(command, sizeof(command), stdin)) {


                //write to server
                write(c_write, command, strlen(command) + 1);

                //exit "gracefully" if disconnect command is given
                if (strcmp(command, "DISCONNECT\n") == 0) {
                    printf("Client: disconnecting from server.\n");
                    write(c_write, command, strlen(command) + 1);
                    break;
                }

                //read from server
                read(c_read, buffer, sizeof(buffer));
                printf("Server: %s\n", buffer);
            }
        }

        close(c_write);
        close(c_read);


    } else {
        fprintf(stderr, "failed sigwait or received wrong signal.\n");
    }

    
    
    return 0;

}

void setup_handler(sigset_t* mask) {
    
    sigemptyset(mask);
    sigaddset(mask, SIGRTMIN + 1);

    if (sigprocmask(SIG_BLOCK, mask, NULL) == -1) {
        perror("client: failed proccessing mask");
        exit(1);
    }
}