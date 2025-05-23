//TODO: client code that can send instructions to server.
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>

char* doc_contents = NULL, *perm = NULL;
size_t doc_length;
uint64_t doc_version;

void receive_doc_data(int c_read);
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
            
            receive_doc_data(c_read);

            char command[256]; 
            while (fgets(command, sizeof(command), stdin)) {
                command[strcspn(command, "\n")] = '\0';

                //write to server
                write(c_write, command, strlen(command) + 1);

                //exit "gracefully" if disconnect command is given
                if (strcmp(command, "DISCONNECT") == 0) {
                    printf("Client: disconnecting from server.");
                    write(c_write, command, strlen(command) + 1);
                    break;
                }

                if (strcmp(command, "DOC?") == 0) {
                    printf("%s\n", doc_contents);
                } 

                if (strcmp(command, "PERM?") == 0) {
                    printf("%s\n", perm);
                }

                //read from server
                read(c_read, buffer, sizeof(buffer));
                printf("Server: %s\n", buffer);
            }

            free(doc_contents);
            free(perm);
        }

        close(c_write);
        close(c_read);


    } else {
        fprintf(stderr, "failed sigwait or received wrong signal.\n");
        exit(1);
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

void receive_doc_data(int c_read) {
    //client authorised 

    /* receiving... 
        role - read/write
        version - doc->version 
        length - doc_len
        document contents

        into
        char* doc_contents = NULL, *perm = NULL;
        size_t doc_length;
        size_t doc_version;

        to read first three lines we will use fgets instead
    */

    int fdcopy = dup(c_read);
    FILE* c_read_FILE = fdopen(fdcopy, "r");

    char buffer[50];

    //gets perm
    fgets(buffer, sizeof(buffer), c_read_FILE); 
    buffer[strcspn(buffer, "\n")] = '\0';
    perm = strdup(buffer);

    //gets version
    fgets(buffer, sizeof(buffer), c_read_FILE);
    sscanf(buffer, "%lu", &doc_version);

    //gets length
    fgets(buffer, sizeof(buffer), c_read_FILE);
    sscanf(buffer, "%zu", &doc_length);

    fclose(c_read_FILE);

    doc_contents = malloc(doc_length + 1);

    //need this to ensure full data is sent over pipe for long documents
    size_t total_read = 0;
    while (total_read < doc_length) {
        ssize_t r = read(c_read, doc_contents + total_read, doc_length - total_read);
        if (r <= 0) {
            perror("read error or EOF");
            break;
        }
        total_read += r;
    }
}