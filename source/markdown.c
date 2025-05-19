#include "../libs/markdown.h"
#include "../libs/document.h"

#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#define SUCCESS 0 
#define INVALID_CURSOR_POS -1
#define DELETED_POSITION -2
#define OUTDATED_VERSION -3

#define GENERAL_FAIL -4

// === Init and Free ===
document *markdown_init(void) {
    return document_init();
}

void markdown_free(document *doc) {
    document_free(doc);
}

// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content) {
    if (doc->version != version) return OUTDATED_VERSION;
    if (pos > doc->size) return INVALID_CURSOR_POS;

    return insert(doc, pos, content) == 0 ? SUCCESS : GENERAL_FAIL;
}

int markdown_delete(document *doc, uint64_t version, size_t pos, size_t len) {
    if (doc->version != version) return OUTDATED_VERSION;
    if (pos > doc->size) return INVALID_CURSOR_POS;

    return delete(doc, pos, len) == 0 ? SUCCESS : GENERAL_FAIL;
}

// === Formatting Commands ===
int markdown_newline(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;
    if (pos > doc->size) return INVALID_CURSOR_POS;

    return insert(doc, pos, "\n") == 0 ? SUCCESS : GENERAL_FAIL;
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
            puts("check_blocking: failed pos");
            return false;
        
        default:
            if (prev_chunk->val == '\n') {
                newline_exists = true;
            }
            break;
    }

    return newline_exists;
}

int markdown_heading(document *doc, uint64_t version, size_t level, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;
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


    return insert(doc, pos, to_insert) == 0 ? SUCCESS : GENERAL_FAIL;
}

int apply_inline(document* doc, uint64_t version, size_t start, size_t end, const char* style) {
    if (doc->version != version) return OUTDATED_VERSION;
    if (start >= end || end > doc->size) return INVALID_CURSOR_POS;

    int insert_suffix = insert(doc, end, style);
    int insert_prefix = insert(doc, start, style);

    if (insert_prefix == -1 || insert_suffix == -1) return GENERAL_FAIL;

    return SUCCESS;
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end) {
    return apply_inline(doc, version, start, end, "**") == SUCCESS ? SUCCESS : GENERAL_FAIL;
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end) {
    return apply_inline(doc, version, start, end, "*") == SUCCESS ? SUCCESS : GENERAL_FAIL;
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;
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

    return insert(doc, pos, buffer) == 0 ? SUCCESS : GENERAL_FAIL;
}

int markdown_ordered_list(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;
    if (pos > doc->size) return INVALID_CURSOR_POS;

    //(1) check if newline exists
    bool newline_exists = check_blocking(doc, pos);

    //(2) scan through backwards and find newline OR last list entry
    chunk* insert_prev, *insert_next;
    int insert_pos = find_position(doc, pos, &insert_prev, &insert_next);

    size_t preceding_digit = 0;

    /* 
    EXPLANATION OF THE BELOW CODE

        we basically iterate backwards two at a time to find when the last previous list entry (or a newline whichever comes first)

        if we find a list entry then we will find its item number

        we will then add 1 to this to find the new list item number

        note that if a newline is found first then preceding digit will remain 0 and thus the new list item number will be a default 1
    */
    while (insert_prev->prev != NULL && insert_prev->val != '\n') {
          
        if (isdigit(insert_prev->prev->val) && (insert_prev->val == '.')) {
            preceding_digit = insert_prev->prev->val - '0';

            //now we exit with the number of the previous list item
            break;
        }

        insert_prev = insert_prev->prev;
            
    }

    char list_item_number = (preceding_digit + 1) + '0';


    //(3) update buffer and insert accordingly
    char buffer[10] = {0};
    size_t index = 0;

    if (!newline_exists) buffer[index++] = '\n';
    buffer[index++] = list_item_number;
    buffer[index++] = '.';
    buffer[index++] = ' ';
    buffer[index++] = '\0';



    // //(4) scan through forwards to find newline OR next list entry
    //     //(4.1) if list entry is found update every subsequent

    //     chunk* prev_char, *next_char;
    //     find_position(doc, pos + size, &prev_char, &next_char)
    
    
    return insert(doc, pos, buffer) == 0 ? SUCCESS : GENERAL_FAIL;
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;
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

    return insert(doc, pos, buffer) == 0 ? SUCCESS : GENERAL_FAIL;
}

int markdown_code(document *doc, uint64_t version, size_t start, size_t end) {
    return apply_inline(doc, version, start, end, "`") == SUCCESS ? SUCCESS : GENERAL_FAIL;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;
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
    
    buffer[index++] = ' ';
    buffer[index++] = '\n';
    buffer[index++] = '\0';

    return insert(doc, pos, buffer) == 0 ? SUCCESS : GENERAL_FAIL;
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url) {
    if (doc == NULL || url == NULL) return GENERAL_FAIL;
    if (version != doc->version) return OUTDATED_VERSION;
    if (start >= end || end > doc->size) return INVALID_CURSOR_POS;


    //(1) create suffix
    size_t size = strlen(url) + 4; //[]()
    char *suffix = malloc(size + 1); //+1 for \0
    snprintf(suffix, size + 1, "](%s)", url);

    //(2) insert prefix and suffix
    int insert_end = insert(doc, end, suffix);
    int insert_start = insert(doc, start, "[");
    free(suffix);

    if (insert_start == -1 || insert_end == -1) return GENERAL_FAIL;

    return SUCCESS;
}

// === Utilities ===
void markdown_print(const document *doc, FILE *stream) {
    if (doc == NULL || stream == NULL) return;

    char* flat = flatten(doc);

    if (flat) {
        fprintf(stream, "%s", flat);
        free(flat);
    }
}

char *markdown_flatten(const document *doc) {
     return flatten(doc);
}

// === Versioning ===
void markdown_increment_version(document *doc) {
   if (doc) doc->version++;
}

