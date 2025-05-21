#ifndef document_H

#define document_H
/**
 * This file is the header file for all the document functions. You will be tested on the functions inside markdown.h
 * You are allowed to and encouraged multiple helper functions and data structures, and make your code as modular as possible. 
 * Ensure you DO NOT change the name of document struct.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    CMD_INSERT,
    CMD_DELETE,
    CMD_NEWLINE,
    CMD_HEADING,
    CMD_BOLD,
    CMD_ITALIC,
    CMD_QUOTE,
    CMD_OLIST,
    CMD_ULIST,
    CMD_CODE,
    CMD_HRULE,
    CMD_LINK

} CommandType;

typedef struct command {
    CommandType type; //enum type
    size_t pos; //may shift with cursor
    size_t len;
    char* content;

    //more niche
    size_t level;
    size_t start;
    size_t end;

    struct command* next;
} Command;

typedef struct chunk{
    // TODO
    char val;
    struct chunk* next;
    struct chunk* prev;

} chunk;

typedef struct document {
    // TODO
    chunk* head;
    chunk* tail;

    size_t size;
    uint64_t version;

    //command queue data structure
    Command* command_head;
    Command* command_tail;

} document;


// Functions from here onwards.

//main
document* document_init(void);
void document_free(document* doc);
int insert(document* doc, size_t pos, const char* content);
int delete(document* doc, size_t pos, size_t len);
char* flatten(const document* doc);

//markdown functions
bool check_blocking(document* doc, size_t pos);
int insert_newline(document *doc, size_t pos);
int insert_heading(document* doc, size_t level, size_t pos);
int apply_inline(document* doc, size_t start, size_t end, const char* style);
int insert_bold(document *doc, size_t start, size_t end);
int insert_italic(document *doc, size_t start, size_t end);
int insert_blockquote(document *doc, size_t pos);
int insert_unordered_list(document *doc, size_t pos);
int insert_code(document *doc, size_t start, size_t end);
int insert_horizontal_rule(document *doc, size_t pos);
int insert_link(document *doc, size_t start, size_t end, const char *url);


//helpers
int find_position(const document* doc, size_t pos, chunk** prev_char, chunk** next_char);
int build_chain(const char* content, chunk** head, chunk** tail);
int enqueue_command(document* doc, CommandType TYPE, size_t pos, size_t len, const char* content, size_t start, size_t end, size_t level);

#endif
