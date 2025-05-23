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

int find_user_perm(const char* username, char* role); //0 = success, -1 = not found


pthread_t* clientele = NULL;
size_t c_index = 0;

void* client_thread(void* args) {

    puts("made it here 1");

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

    puts("made it here after snprintf");

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

    puts("made it here after mkfifo");

    

    puts("made it here before kill");

    kill(cpid, SIGRTMIN + 1);

    puts("made it here after kill");

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

    read(s_read, username, 100);
    printf("%s\n", username);

    char permission[50];
    if(find_user_perm(username, permission) == 0) {
        write(s_write, permission, strlen(permission));
        write(s_write, "\n", 1);

        //start editing the document
        

    } else {
        write(s_write, "Reject UNAUTHORISED.", 19);
        close(s_write);
        close(s_read);
        unlink(c2s);
        unlink(s2c);

        sleep(1);
        free(args);

        pthread_exit(NULL);

    }
    

    close(s_write);
    close(s_read);
    unlink(c2s);
    unlink(s2c);

    free(args);

    puts("DONE WITH CLIENT");
    pthread_exit(NULL);
}

void sig_handler(int signal, siginfo_t* client_info, void* context);


int main(int argc, char* argv[]) {
    if (argc != 2) {
        puts("not enough arguments");
        exit(1);
    }

    int time_interval;
    if (sscanf(argv[1], "%d", &time_interval) != 1) {
        fprintf(stderr, "failed to convert time interval to int.\n");
        exit(1);
    }
    pid_t pid = getpid();
   
    printf("time interval is %d\n", time_interval);
    printf("Server PID: %d\n", pid);

    clientele = calloc(10,sizeof(pthread_t));

    // set up async sig handling
    struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = sig_handler;
        sigemptyset(&sa.sa_mask);

    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        perror("server: sigaction failed.\n");
        exit(1);
    }

    while(1) {
        pause();
    }
    
    return 0;

}

void sig_handler(int signal, siginfo_t* client_info, void* context) {
    (void)signal; (void)context;

    int* arg = malloc(sizeof(int));
    pid_t cpid = client_info->si_pid;

    printf("from server: client pid is %d\n", cpid);
    *arg = cpid;

    printf("Server: received signal from client.\n");
    
    pthread_create(&clientele[c_index++], NULL, client_thread, (void*)arg);

    if (c_index % 10 == 9) {
        pthread_t* temp = realloc(clientele, 2 * (c_index + 1) * sizeof(pthread_t));
        if (!temp) {
            perror("realloc failed");
            exit(1);
        }
        clientele = temp;
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
