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
#include <time.h>

#include <pthread.h>

volatile sig_atomic_t client_shutdown = 0;

char* client_log = NULL;

char* doc_contents = NULL, *perm = NULL;
size_t doc_length = 0;
uint64_t doc_version = 0;

int receive_doc_data(int c_read);
void setup_handler(sigset_t* mask);

void* receiver_func(void* args) {
    int c_file_int = *(int*)args;
    
    FILE* c_read_file = fdopen(c_file_int, "r");
    FILE* log_client = fopen(client_log, "w");

    free(args);

    char line[2048];

    while (!client_shutdown && fgets(line, sizeof(line), c_read_file)) {
        //printf("Server said %s", line);
        
        if (strncmp(line, "ACK", 3) == 0) {
            printf("Server ACK: %s", line);

        } else if (strncmp(line, "VERSION", 7) == 0) {
            printf("Received version update: %s", line);
            // now loop to collect full EDIT logs until END

            fprintf(log_client, "%s", line);

            while (!client_shutdown && fgets(line, sizeof(line), c_read_file)) {
                if (strncmp(line, "END", 3) == 0) {
                    printf("End of broadcast.\n");
                    fprintf(log_client, "%s", line); 
                    fflush(log_client);
                    break;
                }

                if (strncmp(line, "EDIT", 4) == 0) {
                    printf("Log line: %s", line);
                    fprintf(log_client, "%s", line); 
                    fflush(log_client);
                } else {
                    
                    fprintf(stderr, "Unexpected log content: %s", line);
                }
            }



        } else if (strncmp(line, "Reject", 6) == 0) {
            fprintf(stdout, "Server rejected: %s", line);
        } else {
            printf("Server says: %s", line);
        }

        

    }

    fclose(log_client);

    return NULL;
}

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

        //(2.2) get username + random num for client log file
    username = argv[2];
    printf("%d and %s\n", server_pid, username);
    client_log = malloc(sizeof(username) + 100);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand(ts.tv_nsec ^ ts.tv_sec); 

    int rand_id = rand() % 100000; 

    sprintf(client_log, "client_log_%s_%d.txt", username, rand_id);

    //(3) setup signal handling for SIGRTMIN
    sigset_t mask;
    setup_handler(&mask);

    printf("Client pid is: %d\n", getpid());

    //(4) send SIGRTMIN to server from client
    if (kill(server_pid, SIGRTMIN) == -1) {
        perror("Error sending signal");
        exit(1);
    }

    //(5) wait for server and if successful handle signal back

    int signal_received;
    if (sigwait(&mask, &signal_received) == 0 && signal_received == SIGRTMIN + 1) {
        //good to go
        printf("Client: received signal from server.\n");

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

        //WRAP INTO FILE STREAM
        int c_read_copy = dup(c_read);
        int c_write_copy = dup(c_write);

        FILE* c_read_file = fdopen(c_read_copy, "r");
        FILE* c_write_file = fdopen(c_write_copy, "w");

        //write username to server
        fprintf(c_write_file, "%s\n", username);
        fflush(c_write_file);

        //receive back initial doc contents from server

        if (receive_doc_data(c_read) == 0) {

            puts("Client: received server data");

            int* c_file = malloc(sizeof(int));
            *c_file = c_read_copy;

            pthread_t receiver;
            pthread_create(&receiver, NULL, receiver_func, (void*) c_file);

            //char buffer[2048];
            char command[256]; 


            while (fgets(command, sizeof(command), stdin)) {
                command[strcspn(command, "\n")] = '\0';

                
                //====CAT A - USER LOCAL COMMANDS====
                

                if (strcmp(command, "DOC?") == 0) {
                    puts("DOC WAS CALLED CLIENT\n");
                    if (doc_length > 0 && doc_contents) {
                        printf("%s\n", doc_contents);
                    }
                    continue;
                } 

                if (strcmp(command, "PERM?") == 0) {
                    printf("%s\n", perm);
                    continue;
                }

                if (strcmp(command, "LOG?") == 0) {
                    puts("LOG WAS CALLED CLIENT\n");
                    char msg[50];
                    sprintf(msg, "cat %s", client_log);
                    system(msg);
                    continue;
                }


                //==== CAT B - COMMANDS TO SERVER ====

                 //write to server
                
                fwrite(command, 1, strlen(command), c_write_file);
                fwrite("\n", 1, 1, c_write_file);
                fflush(c_write_file);

                //exit "gracefully" if disconnect command is given
                if (strcmp(command, "DISCONNECT") == 0) {
                    printf("Client: disconnecting from server.\n");
                    //write(c_write, command, strlen(command) + 1);
                    //fclose(c_write_file);
                    client_shutdown = 1;
                    // fclose(c_read_file);
                    //pthread_cancel(receiver);
                    pthread_join(receiver, NULL);
                    
                    exit(0); 
                    
                }

               

                //read from server
                //fgets(buffer, sizeof(buffer), c_read_file);
                //printf("Server: %s", buffer);
            }
            
            client_shutdown = 1;
            pthread_join(receiver, NULL);
            free(doc_contents);
            free(perm);

        }

        fclose(c_write_file);
        fclose(c_read_file);


    } else {
        fprintf(stderr, "failed sigwait or received wrong signal.\n");
        exit(1);
    }

    free(client_log);
    
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

//0 for succes, 1 for unauthorised, -1 for error
int receive_doc_data(int c_read) {
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
    
    //(1) wrap read file descriptor into FILE stream 

    int c_read_copy = dup(c_read);
    FILE* c_read_FILE = fdopen(c_read_copy, "r");

    //(2) store initial data into buffer
    char buffer[50];
    
        //(2.1) get permission
        fgets(buffer, sizeof(buffer), c_read_FILE); 
        
        if (strcmp(buffer, "Reject UNAUTHORISED\n") == 0) {
            printf("%s", buffer);
            return 1;
        }
        buffer[strcspn(buffer, "\n")] = '\0';
        perm = strdup(buffer);

        //(2.2) get version
        fgets(buffer, sizeof(buffer), c_read_FILE);
        sscanf(buffer, "%lu", &doc_version);

        //(2.3) get length of document

        fgets(buffer, sizeof(buffer), c_read_FILE);
        sscanf(buffer, "%zu", &doc_length);
        doc_contents = malloc(doc_length + 1);

        //(2.4) get document
        if (doc_length > 0) {
            fread(doc_contents, 1, doc_length, c_read_FILE);
        }
        
        doc_contents[doc_length] = '\0';

    puts("Client: received initial data successfully");

    return 0;
}