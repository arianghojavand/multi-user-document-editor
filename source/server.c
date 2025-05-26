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
#include <semaphore.h>

#include "../libs/markdown.h"

#define MAX_CLIENTS 64


document* doc = NULL;
volatile sig_atomic_t server_shutdown = 0;
pthread_mutex_t shutdown_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t shutdown_cond = PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t change_made = 0;
pthread_mutex_t change_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t increment_cond = PTHREAD_COND_INITIALIZER;

pthread_t* clients = NULL;
size_t c_index = 0;
size_t num_active = 0;
size_t c_capacity = 1;

int client_streams[MAX_CLIENTS];
pthread_mutex_t client_streams_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t client_write_locks[MAX_CLIENTS];

FILE* log_fp;
pthread_mutex_t log_file_lock = PTHREAD_MUTEX_INITIALIZER;
size_t last_line_read = 0;

char** log_messages = NULL;
size_t log_index = 0;
size_t log_capacity = 0;
size_t prev_log_index = 0;
pthread_mutex_t log_list_lock = PTHREAD_MUTEX_INITIALIZER;

int find_user_perm(const char* username, char* role); //0 = success, -1 = not found
void sig_handler(int signal, siginfo_t* client_info, void* context);

void* client_thread(void* args) {
    int cpid = *(int*)args;
    char c2s[50], s2c[50];

    mode_t perm = 0600;

    //(1) establish the pipes
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

    //(2) Send signal to client process to indicate linked pipes
    kill(cpid, SIGRTMIN + 1);

    //(3) open correct end of pipes
        int s_read, s_write;
        //(3.1) open c2s in read mode (s_read)
        if ((s_read = open(c2s, O_RDONLY)) == -1) {
            perror("error opening c2s pipe");
            pthread_exit(NULL);
        }

        //(3.2) open s2c in write mode (s_write)
        if ((s_write = open(s2c, O_WRONLY)) == -1) {
            perror("error opening s2c pipe");
            close(s_read);
            pthread_exit(NULL);
        }

        //(3.3) wrap both in high level FILE I/O stream

        int s_read_copy = dup(s_read);
        int s_write_copy = dup(s_write);

        FILE* s_read_file = fdopen(s_read_copy, "r");
        FILE* s_write_file = fdopen(s_write_copy, "w");

        pthread_mutex_lock(&client_streams_lock);
        client_streams[c_index - 1] = dup(s_write_copy);
        size_t client_num = c_index - 1;
        pthread_mutex_unlock(&client_streams_lock);
        

        printf("THIS IS CLIENT NUMBER: %zu\n", client_num);

    //(4) read username and establish permission
    
    char username[2048];
    fgets(username, sizeof(username), s_read_file);
    username[strcspn(username, "\n")] = '\0';
    printf("Client username: %s\n", username);

    char permission[50];
    if(find_user_perm(username, permission) == 0) {

        //(1) ======== send over document details ========
        char* doc_text = flatten(doc);
        size_t doc_len = strlen(doc_text);
        uint64_t version = doc->version;
        
        /* sending... 
            role - read/write
            version - doc->version 
            length - doc_len
            document contents
        */

        pthread_mutex_lock(&client_write_locks[client_num]);
        printf("Server: client %s authorised - sending document + metadata\n", username);
        printf("Server: perm = %s, version = %lu, length = %zu\n", permission, version, doc_len);
        fprintf(s_write_file, "%s\n", permission); //perm
        fprintf(s_write_file, "%lu\n", version); //version
        fprintf(s_write_file, "%zu\n", doc_len);//length
        fwrite(doc_text, 1, doc_len, s_write_file); //document
        fflush(s_write_file); 
        free(doc_text);
        
        pthread_mutex_unlock(&client_write_locks[client_num]);

        //(2) ======== start listening for client commands ========

        char c_command[256];
        char full_command[256];

        while (fgets(c_command, sizeof(c_command), s_read_file)) {
            c_command[strcspn(c_command, "\n")] = '\0';
            strcpy(full_command, c_command);
            printf("Client %s command: %s\n", username, c_command);

            if (strcmp(c_command, "DISCONNECT") == 0) {
                // puts("User attempting to disconnect");
                // fwrite("\0", 1, 1, s_write_file);
                // fflush(s_write_file); 
                break;
            }

            //(2.1) get the first word of the command and categorize
            char* token = strtok(c_command, " ");

            if (token) {

                int result = 0;

                if (strcmp(permission, "write") != 0) {
                    char* msg = "Reject UNAUTHORISED\n";
                    pthread_mutex_lock(&client_write_locks[client_num]);
                    fprintf(s_write_file, "%s", msg);
                    fflush(s_write_file);
                    pthread_mutex_unlock(&client_write_locks[client_num]);

                    char* log_message = malloc(256);


                    pthread_mutex_lock(&log_list_lock);
                    sprintf(log_message, "EDIT %s %s %s\n", username, full_command, "Reject UNAUTHORISED");
                    log_messages[log_index++] = log_message;
                    //fprintf(log_fp, "EDIT %s %s %s\n", username, full_command, result == 0 ? "SUCCESS" : "Reject");
                    //last_line_read++;

                    while (log_index >= log_capacity) {
                        char** temp = realloc(log_messages, sizeof(void*)* log_capacity * 2);
                        log_messages = temp;
                        log_capacity *= 2;
                    }

                    pthread_mutex_unlock(&log_list_lock);

                    continue;
                }

                if (strcmp(token, "INSERT") == 0) {
                    char* pos_str = strtok(NULL, " ");
                    char* content = strtok(NULL, "");
                    size_t pos;
                    sscanf(pos_str, "%lu", &pos);
                    result = markdown_insert(doc, doc->version, pos, content);
                    //puts("inserted");

                } else if (strcmp(token, "DEL") == 0) {
                    char* pos_str = strtok(NULL, " ");
                    char* len_str = strtok(NULL, "");
                    size_t pos, len;
                    sscanf(pos_str, "%lu", &pos);
                    sscanf(len_str, "%lu", &len);
                    result = markdown_delete(doc, doc->version, pos, len);
                    //puts("deleted");

                } else if (strcmp(token, "NEWLINE") == 0) {
                    char* pos_str = strtok(NULL, "");
                    size_t pos;
                    sscanf(pos_str, "%lu", &pos);
                    result = markdown_newline(doc, doc->version, pos);
                    //puts("newline");

                } else if (strcmp(token, "HEADING") == 0) {
                    char* level_str = strtok(NULL, " ");
                    char* pos_str = strtok(NULL, "");
                    size_t level, pos;
                    sscanf(level_str, "%lu", &level);
                    sscanf(pos_str, "%lu", &pos);
                    result = markdown_heading(doc, doc->version, level, pos);
                    puts("heading");

                } else if (strcmp(token, "BOLD") == 0) {
                    char* start_str = strtok(NULL, " ");
                    char* end_str = strtok(NULL, "");
                    size_t start, end;
                    sscanf(start_str, "%lu", &start);
                    sscanf(end_str, "%lu", &end);
                    result = markdown_bold(doc, doc->version, start, end);
                    //puts("bold");

                } else if (strcmp(token, "ITALIC") == 0) {
                    char* start_str = strtok(NULL, " ");
                    char* end_str = strtok(NULL, "");
                    size_t start, end;
                    sscanf(start_str, "%lu", &start);
                    sscanf(end_str, "%lu", &end);
                    result = markdown_italic(doc, doc->version, start, end);
                    //puts("italic");

                } else if (strcmp(token, "BLOCKQUOTE") == 0) {
                    char* pos_str = strtok(NULL, "");
                    size_t pos;
                    sscanf(pos_str, "%lu", &pos);
                    result = markdown_blockquote(doc, doc->version, pos);
                    puts("blockquote");

                } else if (strcmp(token, "ORDERED_LIST") == 0) {
                    char* pos_str = strtok(NULL, "");
                    size_t pos;
                    sscanf(pos_str, "%lu", &pos);
                    result = markdown_ordered_list(doc, doc->version, pos);
                    puts("ordered list");

                } else if (strcmp(token, "UNORDERED_LIST") == 0) {
                    char* pos_str = strtok(NULL, "");
                    size_t pos;
                    sscanf(pos_str, "%lu", &pos);
                    result = markdown_unordered_list(doc, doc->version, pos);
                    puts("unordered list");

                } else if (strcmp(token, "CODE") == 0) {
                    char* start_str = strtok(NULL, " ");
                    char* end_str = strtok(NULL, "");
                    size_t start, end;
                    sscanf(start_str, "%lu", &start);
                    sscanf(end_str, "%lu", &end);
                    result = markdown_code(doc, doc->version, start, end);
                    puts("code");

                } else if (strcmp(token, "HORIZONTAL_RULE") == 0) {
                    char* pos_str = strtok(NULL, "");
                    size_t pos;
                    sscanf(pos_str, "%lu", &pos);
                    result = markdown_horizontal_rule(doc, doc->version, pos);
                    //puts("horizontal rule");

                } else if (strcmp(token, "LINK") == 0) {
                    char* start_str = strtok(NULL, " ");
                    char* end_str = strtok(NULL, " ");
                    char* url = strtok(NULL, "");
                    size_t start, end;
                    sscanf(start_str, "%lu", &start);
                    sscanf(end_str, "%lu", &end);
                    result = markdown_link(doc, doc->version, start, end, url);
                    //puts("link");
                }

                char* log_message = malloc(256);

                pthread_mutex_lock(&log_list_lock);
                sprintf(log_message, "EDIT %s %s %s\n", username, full_command, result == 0 ? "SUCCESS" : "Reject");
                log_messages[log_index++] = log_message;
                //fprintf(log_fp, "EDIT %s %s %s\n", username, full_command, result == 0 ? "SUCCESS" : "Reject");
                //last_line_read++;

                while (log_index >= log_capacity) {
                    char** temp = realloc(log_messages, sizeof(void*)* log_capacity * 2);
                    log_messages = temp;
                    log_capacity *= 2;
                }

                pthread_mutex_unlock(&log_list_lock);

                // pthread_mutex_lock(&log_file_lock);
                // fprintf(log_fp, "%s", log_message);
                // fflush(log_fp);
                // pthread_mutex_unlock(&log_file_lock);

                

                pthread_mutex_lock(&change_mutex);
                change_made = 1;
                pthread_mutex_unlock(&change_mutex);

                pthread_mutex_lock(&client_write_locks[client_num]);
                fprintf(s_write_file, "ACKNOWLEDGED\n");
                fflush(s_write_file);    
                pthread_mutex_unlock(&client_write_locks[client_num]);
            }
        }
    

    } else {
        char* msg = "Reject UNAUTHORISED";
        pthread_mutex_lock(&client_write_locks[client_num]);
        fprintf(s_write_file, "%s\n", msg);
        fflush(s_write_file);
        pthread_mutex_unlock(&client_write_locks[client_num]);
        sleep(1);
        
    }
    
    close(s_write);
    close(s_read);

    fclose(s_write_file);
    fclose(s_read_file);

    unlink(c2s);
    unlink(s2c);

    free(args);

    printf("DONE WITH CLIENT: %s (pid: %d)\n", username, cpid);

    pthread_mutex_lock(&client_streams_lock);
    
   
    if (client_streams[client_num] != 0) {
        close(client_streams[client_num]); 
        client_streams[client_num] = 0;
    }

    pthread_mutex_unlock(&client_streams_lock);
    num_active--;

    pthread_mutex_destroy(&client_write_locks[client_num]);
    pthread_exit(NULL);
}

void* stdin_responder(void* args) {
    (void)args;

    char buffer[10];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        if (strcmp(buffer, "QUIT\n") == 0) {

            if(num_active == 0) {
                printf("Server: quitting.\n");

                pthread_mutex_lock(&shutdown_lock);
                server_shutdown = 1;
                pthread_cond_signal(&shutdown_cond);
                pthread_mutex_unlock(&shutdown_lock);
                
                break;
            }

            else {
                printf("QUIT Rejected, %ld clients still connected.\n", num_active);
            }
            
        } else if (strcmp(buffer, "LOG?\n") == 0) {
            system("cat log.txt");
            continue;
            
        } else if (strcmp(buffer, "DOC?\n") == 0) {
            if (doc) markdown_print(doc, stdout);
            continue;
            
        }

    }

    return NULL;
}

void broadcast_to_all_clients(const char* line);

void* update_version_func(void* args) {
    int time_interval = *(int*)args;

    while (!server_shutdown) {
        sleep(time_interval);

        if (change_made) {
            markdown_increment_version(doc);

            pthread_mutex_lock(&log_file_lock);

            char version_msg[64];
            snprintf(version_msg, sizeof(version_msg), "VERSION %lu\n", doc->version);
            fprintf(log_fp, "%s", version_msg);
            broadcast_to_all_clients(version_msg);

            while (prev_log_index < log_index) {
                fprintf(log_fp, "%s", log_messages[prev_log_index]);
                broadcast_to_all_clients(log_messages[prev_log_index++]);
            }

            fprintf(log_fp, "END\n");
            broadcast_to_all_clients("END\n");
            fflush(log_fp);

            pthread_mutex_unlock(&log_file_lock);

            pthread_mutex_lock(&change_mutex);
            change_made = 0;
            pthread_mutex_unlock(&change_mutex);
        }
    }

    return NULL;
}

  



int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        fprintf(stderr, "Not enough arguments.\n");
        exit(1);
    }

    log_fp = fopen("log.txt", "w");
    fflush(log_fp);
    
    //fprintf(log_fp, "VERSION 0\n");

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
    doc->version = 0;
    clients = calloc(10,sizeof(pthread_t));

    // (3) set up async sig handling
    struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = sig_handler;
        sigemptyset(&sa.sa_mask);

    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        perror("server: sigaction failed.\n");
        exit(1);
    }
    
    //to hold a log
    log_messages = malloc(sizeof(void*) * 10);
    log_index = 0;
    log_capacity = 10;

    //create thread to listen to stdin
    pthread_t stdin_thread;
    pthread_create(&stdin_thread, NULL, stdin_responder, NULL);

    //create thread to update docs at time intervals
    pthread_t update_version;
    int* time_arg = &time_interval;
    pthread_create(&update_version, NULL, update_version_func, (void*)time_arg);

    pthread_mutex_lock(&shutdown_lock);
    while(!server_shutdown) {
        pthread_cond_wait(&shutdown_cond, &shutdown_lock);
    }
    pthread_mutex_unlock(&shutdown_lock);
    
   
    pthread_cancel(stdin_thread);
    pthread_join(stdin_thread, NULL);
    

   

    //SAVE AND EXIT DONT FORGET TO LOG COMMANDS

    if (change_made) {
        printf("Server quitting, change made, final increment\n");
        markdown_increment_version(doc);
    }
    
    printf("printing final log\n");
    pthread_mutex_lock(&log_file_lock);
    char version_msg[64];
    snprintf(version_msg, sizeof(version_msg), "VERSION %lu\n", doc->version);
    fprintf(log_fp, "%s", version_msg);
    broadcast_to_all_clients(version_msg);

    while (prev_log_index < log_index) {
        fprintf(log_fp, "%s", log_messages[prev_log_index]);
        printf("flushing log: %s\n", log_messages[prev_log_index]);
        broadcast_to_all_clients(log_messages[prev_log_index++]);
    }

    fprintf(log_fp, "END\n");
    broadcast_to_all_clients("END\n");
    fflush(log_fp);
    pthread_mutex_unlock(&log_file_lock);

    for (size_t i = 0; i < c_index; i++) {
        pthread_join(clients[i], NULL);
    
    }
    
    FILE* out = fopen("doc.md", "w");
    if (!out) {
        perror("Failed to open doc.md for writing");
    } else {
        markdown_print(doc, out);
        fclose(out);
        printf("Document saved to doc.md\n");
    }

    for (size_t i = 0; i < log_index; i++) {
        if (log_messages[i]) {
            free(log_messages[i]);
        }
    }

    free(log_messages);
    fclose(log_fp);
    document_free(doc);
    free(clients);
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
        pthread_t* temp = realloc(clients, 2 * (c_capacity) * sizeof(pthread_t));
        c_capacity *= 2;

        if (!temp) {
            perror("realloc failed");
            exit(1);
        }
        clients = temp;
    } 

    if (pthread_create(&clients[c_index], NULL, client_thread, (void*)arg) == 0) {
        pthread_mutex_init(&client_write_locks[c_index], NULL);
        printf("Created a mutex for client num: %zu\n", c_index);
        num_active++;
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

void broadcast_to_all_clients(const char* line) {
    pthread_mutex_lock(&client_streams_lock);

    for (size_t i = 0; i < c_index; i++) {

        if (client_streams[i] == 0) continue;

        FILE* stream = fdopen(client_streams[i], "w");

        pthread_mutex_lock(&client_write_locks[i]);
        if (stream != NULL) {
            fprintf(stream, "%s", line);
            fflush(stream);
        }
        pthread_mutex_unlock(&client_write_locks[i]);
    }

    pthread_mutex_unlock(&client_streams_lock);
}