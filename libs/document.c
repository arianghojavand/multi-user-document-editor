#include "document.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>



document* document_init(void) {

    document* doc = malloc(sizeof(document));

    if (doc == NULL) {return NULL;}

    doc->head = NULL;
    doc->tail = NULL;
    doc->size = 0;
    doc->version = 0;

    return doc;
}

void document_free(document* doc) {
    if (doc == NULL) return;

    chunk* current = doc->head;

    while (current != NULL) {
        chunk* next = current->next;
        free(current);

        current = next;
    }

    free(doc);
}

//helper function to find chunks before and after the insertion/deletion position
//returns 0 if head, 1 if middle, 2 if tail, -1 if invalid
int find_position(const document* doc, size_t pos, chunk** prev_char, chunk** next_char) {
    chunk* current = doc->head;
    size_t cursor = 0;

    if (pos == 0) {
        //at head
        *prev_char = NULL;
        *next_char = current;
        return 0;
    } else if (pos == doc->size) {
        //at tail
        *prev_char = doc->tail;
        *next_char = NULL;
        return 2;
    } else {

        //in the middle
        while (cursor < pos) {
            if (current == NULL) return -1;
            cursor++;
            current = current->next;
        }

        *prev_char = current->prev;
        *next_char = current;

        return 1;
    }
    
}

int build_chain(const char* content, chunk** head, chunk** tail) {
    if (content == NULL) return -1;
    
    
    
    size_t content_len = strlen(content);
    chunk* current_chunk = malloc(sizeof(chunk));
    current_chunk->val = content[0];
    current_chunk->prev = NULL;
    *head = current_chunk;

    if (content_len == 1) {
        current_chunk->next = NULL;
        *tail = current_chunk;
        return 0;
    } 

    size_t i = 1;

    while (content[i] != '\0') {
        chunk* new_chunk = malloc(sizeof(chunk));
        new_chunk->val = content[i];

        new_chunk->next = NULL;
        new_chunk->prev = current_chunk;
        current_chunk->next = new_chunk;

        current_chunk = current_chunk->next;

        i++;

    }

    *tail = current_chunk;
    return 0;
}

//return 0 if successful, -1 if invalid
int insert(document* doc, size_t pos, const char* content) {
    if (doc == NULL || content == NULL || pos > doc->size) {
        puts("doc insert: invalid document, empty content, or invalid position");
        return -1;
    }

    size_t content_len = strlen(content);

    chunk* chain_head = NULL, * chain_tail = NULL;
    int chain = build_chain(content, &chain_head, &chain_tail);

    if (chain == -1){
        puts("build chain failed");
        return -1;
    }

    //if this is the first insertion
    if (doc->size == 0) {

        doc->size = content_len;
        doc->head = chain_head;
        doc->tail = chain_tail;
        return 0;
    }

    //otherwise
    else {
        chunk* prev_char = NULL, * next_char = NULL;

        int pos_chunks = find_position(doc, pos, &prev_char, &next_char);
        if (pos_chunks == -1) {
            puts("pos_chunks failed");
            return -1;
        }


        if (pos_chunks == 0) {
            //inserting at the head
            doc->head->prev = chain_tail;
            chain_tail->next = doc->head;


            doc->head = chain_head;

        } else if (pos_chunks == 1) {
            //inserting in the middle
            prev_char->next = chain_head;
            chain_head->prev = prev_char;

            next_char->prev = chain_tail;
            chain_tail->next = next_char;


        } else if (pos_chunks == 2) {
            //inserting tail

            doc->tail->next = chain_head;
            chain_head->prev = doc->tail;

            doc->tail = chain_tail;
        }

        doc->size += content_len;
        


    }

    //doc->version++;
    return 0;
}

int delete(document* doc, size_t pos, size_t len) { 
    if (doc == NULL || len == 0 || pos >= doc->size) return -1;
    if (pos + len > doc->size) len = doc->size - pos;

    //(1) find starting and ending positions of deletion rate using helper
    chunk* start_prev, *start_next, *end_prev, *end_next;

    int starting = find_position(doc, pos, &start_prev, &start_next);
    int ending = find_position(doc, pos + len, &end_prev, &end_next);

    //(2) delete portions from start_next to end_prev

    chunk* current_chunk = start_next;

    while (current_chunk != end_next) {
        chunk* temp = current_chunk;
        current_chunk = current_chunk->next;
        free(temp);
    }

    //(3) link up

    if (starting == 0 && ending == 2) {
        //deleting whole doc
        doc->head = NULL;
        doc->tail = NULL;

    } else if (starting == 0) {
        doc->head = end_next;

    } else if (ending == 2) {
        doc->tail = start_prev;

    } else {
        start_prev->next = end_next;
        end_next->prev = start_prev;

    }

    doc->size -= len;
    //doc->version++;

    return 0;
}

char* flatten(const document* doc) {
    if (doc == NULL || doc->size == 0) {
        char *empty = malloc(1);
        empty[0] = '\0';
        return empty;
    }

    char* buffer = malloc(doc->size + 1);

    chunk* current_chunk = doc->head;
    size_t buffer_index = 0;

    while (current_chunk != NULL) {
        buffer[buffer_index++] = current_chunk->val;
        current_chunk = current_chunk->next;
    }

    //null terminate at the end
    buffer[buffer_index] = '\0';

    return buffer;
}