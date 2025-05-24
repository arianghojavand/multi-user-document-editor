#define _POSIX_C_SOURCE 200809L

#include "server_commands.h"
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>



//if NULL is passed into first param then we create a new list
CommandList* add_command(CommandList* list, CommandItem* cmd) {

    //we must make a new one
    if (!list) {
        CommandList* list = calloc(1, sizeof(CommandList));

        if (!list){
            fprintf(stderr, "error in command list struct alloc");
            exit(1);
        }

        list->commands = malloc(10 * sizeof(void*));

        if (!list->commands){
            fprintf(stderr, "error in command list commands alloc");
            exit(1);
        }

        list->index = 0;
        list->capacity = 10;

        list->commands[list->index++] = cmd;
        return list;
    }

    while (list->index + 1 >= list->capacity ) {
        
        CommandItem** temp = realloc(list->commands, sizeof(void*) * list->capacity * 2);

        if (!temp) {
            fprintf(stderr, "error in command list realloc.\n");
            exit(1);
        }
        
        list->commands = temp;
        list->capacity *= 2;
    }

    list->commands[list->index++] = cmd;

    return list;
} 



CommandItem* create_command(CommandType cmd_type, size_t pos, const char* content, size_t len, size_t level, size_t start, size_t end) {
    CommandItem* cmd_item = malloc(sizeof(CommandItem));
    if (!cmd_item) {
        fprintf(stderr, "error in command alloc");
        exit(1);
    }

    /*
    CommandType type;
    size_t pos;
    size_t len;
    char* content;

    size_t level;
    size_t start;
    size_t end;

    uint64_t timestamp;
   
    */

    cmd_item->type = cmd_type;
    cmd_item->pos = pos;
    cmd_item->len = len;

    if (content) {
        cmd_item->content = strdup(content);
    } else {
        cmd_item->content = NULL;
    }

    cmd_item->level = level;
    cmd_item->start = start;
    cmd_item->end = end;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    cmd_item->timestamp = (uint64_t)(ts.tv_sec * 1000000000L + ts.tv_nsec);

    
    return cmd_item;
}


void swap_commands(CommandItem** cmd_x, CommandItem** cmd_y) {
    CommandItem* temp = *cmd_x;
    *cmd_x = *cmd_y;
    *cmd_y = temp;
}

void time_sort(CommandList* list) {
    size_t i, j;

    for (i = 0; i < list->index; i++) {
        size_t swapped = 0;

        for (j = 0; j < list->index - i - 1; j++) {
            if (list->commands[j]->timestamp > list->commands[j+1]->timestamp) {
                swap_commands(&list->commands[j], &list->commands[j+1]);
                swapped = 1;
            }
        }

        if (swapped == 0) {
            break;
        }
    }
    
}


void free_command_list(CommandList* list){
    if (!list) return;

    for (size_t i = 0; i < list->index; i++) {
        CommandItem* cmd = list->commands[i];
        if (cmd) {
            free(cmd->content); 
            free(cmd);           
        }
    }

    free(list->commands);      
    free(list);            

}