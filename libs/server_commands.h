

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "document.h"


typedef struct  {
    CommandType type;
    size_t pos;
    size_t len;
    char* content;

    size_t level;
    size_t start;
    size_t end;

    uint64_t timestamp;

} CommandItem;

//array list
typedef struct command_list {
    CommandItem** commands;  //list of command items
    size_t index;
    size_t capacity;
} CommandList;


CommandList* add_command(CommandList* list, CommandItem* cmd); //if NULL is passed into first param then we create a new list
CommandItem* create_command(CommandType cmd_type, size_t pos, const char* content, size_t len, size_t level, size_t start, size_t end);
void swap_commands(CommandItem** cmd_x, CommandItem** cmd_y);

void time_sort(CommandList* list);
void free_command_list(CommandList* list);
