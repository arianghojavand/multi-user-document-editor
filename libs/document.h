#ifndef DOCUMENT_H

#define DOCUMENT_H
/**
 * This file is the header file for all the document functions. You will be tested on the functions inside markdown.h
 * You are allowed to and encouraged multiple helper functions and data structures, and make your code as modular as possible. 
 * Ensure you DO NOT change the name of document struct.
 */

#include <stdint.h>
#include <stddef.h>

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

} document;

// Functions from here onwards.

//main
document* document_init(void);
void document_free(document* doc);
int insert(document* doc, size_t pos, const char* content);
int delete(document* doc, size_t pos, size_t len);
char* flatten(const document* doc);


//helpers
int find_position(const document* doc, size_t pos, chunk** prev_char, chunk** next_char);
int build_chain(const char* content, chunk** head, chunk** tail);

#endif
