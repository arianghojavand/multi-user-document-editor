// TODO: server code that manages the document and handles client instructions
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>

#include "../libs/markdown.h"

document* doc = NULL;
volatile sig_atomic_t server_shutdown = 0;
pthread_mutex_t shutdown_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t shutdown_cond = PTHREAD_COND_INITIALIZER;

pthread_t* clientele = NULL;
size_t c_index = 0;
size_t c_capacity = 1;

int find_user_perm(const char* username, char* role); //0 = success, -1 = not found
void sig_handler(int signal, siginfo_t* client_info, void* context);

void* client_thread(void* args) {

    puts("hello");
    int cpid = *(int*)args;
    char c2s[50], s2c[50];

    mode_t perm = 0600;

    if (snprintf(c2s, 50, "FIFO_C2S_%d", cpid) == -1) {
        fprintf(stderr, "error printing pipe string c2s.\n");
        exit(1);
    }

    if (snprintf(s2c, 50, "FIFO_S2C_%d", cpid) == -1) {
        fprintf(stderr, "error printing pipe string s2c.\n");
        exit(1);
    }

    unlink(c2s);
    if (mkfifo(c2s, perm) == -1) {
        fprintf(stderr, "error making c2s pipe.\n");
        exit(1);
    }

    unlink(s2c);
    if (mkfifo(s2c, perm) == -1) {
        fprintf(stderr, "error making s2c pipe.\n");
        exit(1);
    }


    kill(cpid, SIGRTMIN + 1);

    int s_read, s_write;
    if ((s_read = open(c2s, O_RDONLY)) == -1) {
        perror("error opening c2s pipe");
        pthread_exit(NULL);
    }
    if ((s_write = open(s2c, O_WRONLY)) == -1) {
        perror("error opening s2c pipe");
        close(s_read);
        pthread_exit(NULL);
    }

    //read username
    char username[2048];

    read(s_read, username, sizeof(username));
    printf("%s\n", username);

    char permission[50];
    if(find_user_perm(username, permission) == 0) {

        char* doc_text = flatten(doc);
        size_t doc_len = strlen(doc_text);
        uint64_t version = doc->version;
        
        /* sending... 
            role - read/write
            version - doc->version 
            length - doc_len
            document contents
        */

        puts("check");
        int s_write_copy = dup(s_write);
        FILE* s_write_FILE = fdopen(s_write_copy, "w");

        printf("%s\n", permission);
        fprintf(s_write_FILE, "%s\n", permission);
    
        fprintf(s_write_FILE, "%lu\n", version);
        printf("%lu\n", version);
        fprintf(s_write_FILE, "%zu\n", doc_len);
        printf("%zu\n", doc_len);
        //fflush(s_stream);  

        fflush(s_write_FILE); 

        write(s_write, doc_text, doc_len);  

        puts("check again");

        //start editing the document

        sleep(60);
        

    } else {
        char* msg = "Reject UNAUTHORISED.\n";
        write(s_write, msg, strlen(msg) + 1);
        // close(s_write);
        // close(s_read);
        // unlink(c2s);
        // unlink(s2c);

        sleep(1);
        // free(args);

        // printf("DONE WITH CLIENT: %d\n", cpid);
        // c_index--;
        // pthread_exit(NULL);
    }
    

    close(s_write);
    close(s_read);
    unlink(c2s);
    unlink(s2c);

    free(args);

    printf("DONE WITH CLIENT: %d\n", cpid);
    c_index--;
    pthread_exit(NULL);
}

void* stdin_responder(void* args) {
    (void)args;

    char buffer[10];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        if (strcmp(buffer, "QUIT\n") == 0) {

            if(c_index == 0) {
                printf("Server: quitting.\n");

                pthread_mutex_lock(&shutdown_lock);
                server_shutdown = 1;
                pthread_cond_signal(&shutdown_cond);
                pthread_mutex_unlock(&shutdown_lock);

                break;
            }

            else {
                printf("QUIT Rejected, %ld clients still connected.\n", c_index);
            }
            
        }

    }

    return NULL;
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Not enough arguments.\n");
        exit(1);
    }

    //(1) get the time interval and pid
    int time_interval;
    if (sscanf(argv[1], "%d", &time_interval) != 1) {
        fprintf(stderr, "failed to convert time interval to int.\n");
        exit(1);
    }
    pid_t pid = getpid();
   
    //printf("time interval is %d\n", time_interval);
    printf("Server PID: %d\n", pid);

    //(2) create doc and set up array of threads
    doc = document_init();
    clientele = calloc(10,sizeof(pthread_t));

    // (3) set up async sig handling
    struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = sig_handler;
        sigemptyset(&sa.sa_mask);

    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        perror("server: sigaction failed.\n");
        exit(1);
    }
    
    //create thread to listen to stdin
    pthread_t stdin_thread;
    pthread_create(&stdin_thread, NULL, stdin_responder, NULL);

    pthread_mutex_lock(&shutdown_lock);
    while(!server_shutdown) {
        pthread_cond_wait(&shutdown_cond, &shutdown_lock);
    }
    pthread_mutex_unlock(&shutdown_lock);
    
   
    pthread_cancel(stdin_thread);
    pthread_join(stdin_thread, NULL);
    

    for (size_t i = 0; i < c_index; i++) {
        pthread_join(clientele[i], NULL);
    
    }

    FILE* out = fopen("doc.md", "w");
    if (!out) {
        perror("Failed to open doc.md for writing");
    } else {
        markdown_print(doc, out);
        fclose(out);
        printf("Document saved to doc.md\n");
    }
    
    document_free(doc);
    free(clientele);
    return 0;
}

void sig_handler(int signal, siginfo_t* client_info, void* context) {
    (void)signal; (void)context;

    int* arg = malloc(sizeof(int));
    pid_t cpid = client_info->si_pid;

    //printf("from server: client pid is %d\n", cpid);
    *arg = cpid;

    //printf("Server: received signal from client.\n");
    while (2* c_index >= c_capacity) {
        pthread_t* temp = realloc(clientele, 2 * (c_capacity) * sizeof(pthread_t));
        c_capacity *= 2;

        if (!temp) {
            perror("realloc failed");
            exit(1);
        }
        clientele = temp;
    } 

    if (pthread_create(&clientele[c_index], NULL, client_thread, (void*)arg) == 0) {
        c_index++;
    } else {
        perror("pthread_create failed");
        free(arg);
    }
}

int find_user_perm(const char* username, char* role) {
    FILE *fptr = fopen("roles.txt", "r");
    if (!fptr) return -1;

    char buffer[2048];

   
    while (fgets(buffer, sizeof(buffer), fptr)) {
        char *name = strtok(buffer, " \n");
        char *perm = strtok(NULL, " \n");

        if (strcmp(name, username) == 0) {
            strcpy(role, perm);
            fclose(fptr);
            return 0;
        }
    }
    fclose(fptr);
    return -1;

}
