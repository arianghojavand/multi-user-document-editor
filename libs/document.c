#include "document.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>

#define SUCCESS 0
#define INVALID_CURSOR_POS -1
#define DELETED_POSITION -2
#define OUTDATED_VERSION -3

document* document_init(void) {

    document* doc = malloc(sizeof(document));

    if (doc == NULL) {return NULL;}

    doc->head = NULL;
    doc->tail = NULL;
    doc->size = 0;
    doc->version = 0;

    doc->command_head = NULL;
    doc->command_tail = NULL;

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

    while (doc->command_head) {
        Command* temp = doc->command_head->next;
        free(doc->command_head);
        doc->command_head = temp;
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
    return SUCCESS;
}

int delete(document* doc, size_t pos, size_t len) { 
    if (doc == NULL || pos > doc->size) {
        fprintf(stderr, "delete: invalid position, or null document\n");
        return -1;
    }

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
        if (end_next) end_next->prev = NULL;

    } else if (ending == 2) {
        doc->tail = start_prev;
        if (start_prev) start_prev->next = NULL;

    } else {
        if (start_prev) start_prev->next = end_next;
        if (end_next) end_next->prev = start_prev;

    }

    doc->size -= len;
    //doc->version++;

    return SUCCESS;
}

bool check_blocking(document* doc, size_t pos) {
    bool newline_exists = false;
    
    chunk* prev_chunk, *next_chunk;
    int find_pos = find_position(doc, pos, &prev_chunk, &next_chunk);

    switch (find_pos) {
        case 0:
            newline_exists = true;
            break;
        case -1:
            fprintf(stderr, "check_blocking: failed to find position");
            return false;
        
        default:
            if (prev_chunk->val == '\n') {
                newline_exists = true;
            }
            break;
    }

    return newline_exists;
}

int insert_newline(document *doc, size_t pos) {
    
    if (pos > doc->size) return INVALID_CURSOR_POS;

    return insert(doc, pos, "\n") == 0 ? SUCCESS : -1;
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

    while (current_chunk) {
        buffer[buffer_index++] = current_chunk->val;
        current_chunk = current_chunk->next;
    }

    //null terminate at the end
    buffer[buffer_index] = '\0';

    return buffer;
}

int insert_heading(document* doc, size_t level, size_t pos) {
    
    if (level < 1 || level > 3) return INVALID_CURSOR_POS;
    if (pos > doc->size) return INVALID_CURSOR_POS;


    //(1) establish whether or not it is on a newline
    bool newline_exists = check_blocking(doc, pos);

    //(2) create char array to be inserted (size 10 will be more than enough)

    char to_insert[10] = {0};
    size_t i = 0;

    if (!newline_exists) {
        to_insert[i++] = '\n';
    } 

    for (size_t j = 0; j < level; j++) {
        to_insert[i++] = '#';
    }

    to_insert[i++] = ' ';
    to_insert[i++] = '\0';
    
    return insert(doc, pos, to_insert) == 0 ? SUCCESS : -1;
}

int apply_inline(document* doc, size_t start, size_t end, const char* style) {
    
    if (start >= end || end > doc->size) return INVALID_CURSOR_POS;

    int insert_suffix = insert(doc, end, style);
    int insert_prefix = insert(doc, start, style);

    if (insert_prefix == -1 || insert_suffix == -1) return -1;

    return SUCCESS;
}

int insert_bold(document *doc, size_t start, size_t end) {
    return apply_inline(doc, start, end, "**") == SUCCESS ? SUCCESS : -1;
}

int insert_italic(document *doc, size_t start, size_t end) {
    return apply_inline(doc, start, end, "*") == SUCCESS ? SUCCESS : -1;
}

int insert_blockquote(document *doc, size_t pos) {
    
    if (pos > doc->size) return INVALID_CURSOR_POS;

    //(1) check if newline requirement is met
    bool newline_exists = check_blocking(doc, pos);

    //(2) create insert string and fill accordingly
    char buffer[10] = {0};
    size_t index = 0;

    if (!newline_exists) buffer[index++] = '\n';
    buffer[index++] = '>';
    buffer[index++] = ' ';
    buffer[index++] = '\0';

    return insert(doc, pos, buffer) == 0 ? SUCCESS : -1;
}

int insert_unordered_list(document *doc, size_t pos) {
    
    if (pos > doc->size) return INVALID_CURSOR_POS;

    //(1) check if newline exists
    bool newline_exists = check_blocking(doc, pos);

    //(2) create buffer and insert

    char buffer[10] = {0};
    size_t index = 0;

    if (!newline_exists) buffer[index++] = '\n';
    buffer[index++] = '-';
    buffer[index++] = ' ';
    buffer[index++] = '\0';

    return insert(doc, pos, buffer) == 0 ? SUCCESS : -1;
}

int insert_code(document *doc, size_t start, size_t end) {
    return apply_inline(doc, start, end, "`") == SUCCESS ? SUCCESS : -1;
}

int insert_horizontal_rule(document *doc, size_t pos) {
   
    if (pos > doc->size) return INVALID_CURSOR_POS;

    //(1) check if newline requirement is met
    bool newline_exists = check_blocking(doc, pos);

    //(2) create insert string and fill accordingly
    char buffer[10] = {0};
    size_t index = 0;

    if (!newline_exists) buffer[index++] = '\n';
    
    for (size_t j = 0; j < 3; j++) {
        buffer[index++] = '-';
    }
    
    //buffer[index++] = ' ';
    buffer[index++] = '\n';
    buffer[index++] = '\0';

    return insert(doc, pos, buffer) == 0 ? SUCCESS : -1;
}

int insert_link(document *doc, size_t start, size_t end, const char *url) {
    if (doc == NULL || url == NULL) return -1;
    
    if (start >= end || end > doc->size) return INVALID_CURSOR_POS;


    //(1) create suffix
    size_t size = strlen(url) + 4; //[]()
    char *suffix = malloc(size + 1); //+1 for \0
    snprintf(suffix, size + 1, "](%s)", url);

    //(2) insert prefix and suffix
    int insert_end = insert(doc, end, suffix);
    int insert_start = insert(doc, start, "[");
    free(suffix);

    if (insert_start == -1 || insert_end == -1) return -1;

    return SUCCESS;
}

// === Utilities ===
void print_doc(const document *doc, FILE *stream) {
    if (doc == NULL || stream == NULL) return;

    char* flat = flatten(doc);

    if (flat) {
        fprintf(stream, "%s", flat);
        free(flat);
    }
}

int enqueue_command(document* doc, CommandType TYPE, size_t pos, size_t len, const char* content, size_t start, size_t end, size_t level) {
    Command* cmd = malloc(sizeof(Command));
    
    


    //fill out struct
    cmd->type = TYPE;
    cmd->pos = pos;
    cmd->len = len;
   

    if (content) {
        cmd->content = strdup(content);
    } else {
        cmd->content = NULL;
    }
    cmd->next = NULL;
    
    cmd->start = start;
    cmd->end = end;
    cmd->level = level;

    //update command queue in main
    
   

    if (!doc->command_head) {
        doc->command_head = cmd;
        doc->command_tail = cmd;
    } else {
        doc->command_tail->next = cmd;
        doc->command_tail = cmd;
    }
    
    return SUCCESS;

}