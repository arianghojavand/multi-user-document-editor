// TODO: server code that manages the document and handles client instructions

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char* argv[]) {
    printf("num args %d\n", argc);


    if (argc != 2) {
        puts("not enough arguments");
        exit(1);
    }

    char* time_interval = argv[1];

    printf("time interval is %s\n", time_interval);

    pid_t pid = getpid();
    printf("Server PID: %d\n", pid);
    
    return 0;

}