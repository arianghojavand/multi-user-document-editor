#ifndef DOCUMENT_H

#define DOCUMENT_H
/**
 * This file is the header file for all the Document functions. You will be tested on the functions inside markdown.h
 * You are allowed to and encouraged multiple helper functions and data structures, and make your code as modular as possible. 
 * Ensure you DO NOT change the name of Document struct.
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

} Chunk;

typedef struct document {
    // TODO
    Chunk* head;
    Chunk* tail;

    size_t size;
    uint64_t version;

    //command queue data structure
    Command* command_head;
    Command* command_tail;

} Document;


// Functions from here onwards.

//main
Document* document_init(void);
void document_free(Document* doc);
int insert(Document* doc, size_t pos, const char* content);
int delete(Document* doc, size_t pos, size_t len);
char* flatten(const Document* doc);

//markdown functions
bool check_blocking(Document* doc, size_t pos);
int insert_newline(Document *doc, size_t pos);
int insert_heading(Document* doc, size_t level, size_t pos);
int apply_inline(Document* doc, size_t start, size_t end, const char* style);
int insert_bold(Document *doc, size_t start, size_t end);
int insert_italic(Document *doc, size_t start, size_t end);
int insert_blockquote(Document *doc, size_t pos);
int insert_unordered_list(Document *doc, size_t pos);
int insert_code(Document *doc, size_t start, size_t end);
int insert_horizontal_rule(Document *doc, size_t pos);
int insert_link(Document *doc, size_t start, size_t end, const char *url);


//helpers
int find_position(const Document* doc, size_t pos, Chunk** prev_char, Chunk** next_char);
int build_chain(const char* content, Chunk** head, Chunk** tail);
int enqueue_command(Document* doc, CommandType TYPE, size_t pos, size_t len, const char* content, size_t start, size_t end, size_t level);

#endif
