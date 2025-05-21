#include "../libs/markdown.h"
#include "../libs/document.h"

#include <stdbool.h>
#include <ctype.h>

#include <stdlib.h>
#include <string.h>


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
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_INSERT, pos, 0, content, 0, 0, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_delete(document *doc, uint64_t version, size_t pos, size_t len) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_DELETE, pos, len, NULL, 0, 0, 0) == SUCCESS ? SUCCESS : -1; 
}

// === Formatting Commands ===
int markdown_newline(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_NEWLINE, pos, 0, NULL, 0, 0, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_heading(document *doc, uint64_t version, size_t level, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_HEADING, pos, 0, NULL, 0, 0, level) == SUCCESS ? SUCCESS : -1;
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_BOLD, 0, 0, NULL, start, end, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_ITALIC, 0, 0, NULL, start, end, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_QUOTE, pos, 0, NULL, 0, 0, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_ordered_list(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;

    (void)doc; (void)version; (void)pos;
    // if (version != doc->version) return OUTDATED_VERSION;
    // if (pos > doc->size) return INVALID_CURSOR_POS;

    // //(1) check if newline exists
    // bool newline_exists = check_blocking(doc, pos);

    // //(2) scan through backwards and find newline OR last list entry
    // chunk* insert_prev, *insert_next;
    // int insert_pos = find_position(doc, pos, &insert_prev, &insert_next);

    // size_t preceding_digit = 0;

    // /* 
    // EXPLANATION OF THE BELOW CODE

    //     we basically iterate backwards two at a time to find when the last previous list entry (or a newline whichever comes first)

    //     if we find a list entry then we will find its item number

    //     we will then add 1 to this to find the new list item number

    //     note that if a newline is found first then preceding digit will remain 0 and thus the new list item number will be a default 1
    // */
    // while (insert_prev->prev != NULL && insert_prev->val != '\n') {
          
    //     if (isdigit(insert_prev->prev->val) && (insert_prev->val == '.')) {
    //         preceding_digit = insert_prev->prev->val - '0';

    //         //now we exit with the number of the previous list item
    //         break;
    //     }

    //     insert_prev = insert_prev->prev;
            
    // }

    // char list_item_number = (preceding_digit + 1) + '0';


    // //(3) update buffer and insert accordingly
    // char buffer[10] = {0};
    // size_t index = 0;

    // if (!newline_exists) buffer[index++] = '\n';
    // buffer[index++] = list_item_number;
    // buffer[index++] = '.';
    // buffer[index++] = ' ';
    // buffer[index++] = '\0';



    // // //(4) scan through forwards to find newline OR next list entry
    // //     //(4.1) if list entry is found update every subsequent

    // //     chunk* prev_char, *next_char;
    // //     find_position(doc, pos + size, &prev_char, &next_char)
    
    
    // return insert(doc, pos, buffer) == 0 ? SUCCESS : GENERAL_FAIL;
    return SUCCESS;
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_ULIST, pos, 0, NULL, 0, 0, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_code(document *doc, uint64_t version, size_t start, size_t end) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_CODE, 0, 0, NULL, start, end, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_HRULE, pos, 0, NULL, 0, 0, 0) == SUCCESS ? SUCCESS : -1;
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url) {
    if (version != doc->version) return OUTDATED_VERSION;

    return enqueue_command(doc, CMD_LINK, 0, 0, url, start, end, 0) == SUCCESS ? SUCCESS : -1;
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
   //puts("MADE IT HERE");
   //(1) go iteratively through cmds (from head to tail)

    typedef struct {
        size_t start;
        size_t end;
    } DeletedRange;

    DeletedRange** deleted_range_list = calloc(50, sizeof(void*));
    size_t dr_index = 0;

    Command* current_command = doc->command_head;
    while (current_command) {
        //puts("AND HERE");
        switch (current_command->type) {
            case CMD_INSERT:
                //puts("AND HERE AGAIN");

                for (size_t i = 0; i < dr_index; i++) {
                    if (current_command->pos >= deleted_range_list[i]->start
                        && current_command->pos < deleted_range_list[i]->end) {

                            current_command->pos = deleted_range_list[i]->start;
                            break;

                    }
                }

                insert(doc, current_command->pos, current_command->content);

                break;
            
            case CMD_DELETE:
                delete(doc, current_command->pos, current_command->len);
                DeletedRange* deleted_range = malloc(sizeof(DeletedRange));
                deleted_range->start = current_command->pos;
                deleted_range->end = current_command->pos + current_command->len;

                deleted_range_list[dr_index++] = deleted_range;

                if (dr_index % 50 == 0) {
                    deleted_range_list = realloc(deleted_range_list, 2 * dr_index * sizeof(void*));
                }

                break;
            
            case CMD_NEWLINE:
                insert_newline(doc, current_command->pos);
                break;

            case CMD_HEADING:
                insert_heading(doc, current_command->level, current_command->pos);
                break;

            case CMD_BOLD:
                insert_bold(doc, current_command->start, current_command->end);
                break;
            
            case CMD_ITALIC:
                insert_italic(doc, current_command->start, current_command->end);
                break;
            
            case CMD_QUOTE:
                insert_blockquote(doc, current_command->pos);
                break;

            case CMD_OLIST:
                insert_unordered_list(doc, current_command->pos);
                break;

            case CMD_ULIST:
                insert_unordered_list(doc, current_command->pos);
                break;

            case CMD_CODE:
                insert_code(doc, current_command->start, current_command->end);
                break;

            case CMD_HRULE:
                insert_horizontal_rule(doc, current_command->pos);
                break;

            case CMD_LINK:
                insert_link(doc, current_command->start, current_command->end, current_command->content);
                break;
            
            default: //command not recognised
                fprintf(stderr, "markdown_inc_ver: command not recognisded\n");
                break;

        }


        Command* temp = current_command;
        current_command = current_command->next;

        if (temp->content) {
            free(temp->content);
        }
        
        free(temp);



    }
    doc->command_head = doc->command_tail = NULL;

    for (size_t i = 0; i < dr_index; i++) {
        if (!deleted_range_list[i]) continue;

        free(deleted_range_list[i]);
    }

    free(deleted_range_list);
    
    if (doc) doc->version++;
}

// int main(void) {
//     // document *doc = markdown_init();

//     // puts("=== BATCH 1: Initial Inserts ===");
//     // markdown_insert(doc, 0, 0, "fox");         // Insert "fox" at pos 0:          → "fox"
//     // markdown_insert(doc, 0, 0, "The ");        // Insert "The " at pos 0:         → "The fox"
//     // markdown_insert(doc, 0, 4, "quick ");      // Insert "quick " at pos 4:       → "The quick fox"
//     // markdown_insert(doc, 0, 10, "brown ");     // Insert "brown " at pos 10:      → "The quick brown fox"
//     // markdown_insert(doc, 0, 19, " jumps");      // Insert "jumps" at pos 16:       → "The quick brown jumps fox"
//     // markdown_insert(doc, 0, 25, " over");      // Insert " over" at pos 21:       → "The quick brown jumps over fox"
//     // markdown_insert(doc, 0, 30, " the");       // Insert " the" at pos 26:        → "The quick brown jumps over the fox"
//     // markdown_insert(doc, 0, 34, " lazy");      // Insert " lazy" at pos 30:       → "The quick brown jumps over the lazy fox"
//     // markdown_insert(doc, 0, 39, " dog");       // Insert " dog" at pos 35:        → "The quick brown jumps over the lazy fox dog"

//     // puts("Flatten before increment (expect empty):");
//     // char *before1 = flatten(doc);
//     // printf("[%s]\n", before1);
//     // free(before1);

//     // puts("Incrementing version to 1...");
//     // markdown_increment_version(doc);
//     // markdown_print(doc, stdout);
//     // printf("\n--- Expected v1: [The quick brown jumps over the lazy dog]\n\n");

//     // puts("=== BATCH 2: Deleting and Adding ===");
//     // markdown_delete(doc, 1, 4, 6);      // Delete "quick "
//     // markdown_insert(doc, 1, 4, "slow "); // Insert "slow " at former quick position
//     // markdown_delete(doc, 1, 34, 4);     // Delete "lazy"
//     // markdown_insert(doc, 1, 34, "smart"); // Replace with "smart"

//     // puts("Flatten before increment (still v1):");
//     // char *before2 = flatten(doc);
//     // printf("[%s]\n", before2);
//     // free(before2);

//     // puts("Incrementing version to 2...");
//     // markdown_increment_version(doc);
//     // markdown_print(doc, stdout);
//     // printf("\n--- Expected v2: [The slow brown jumps over the smart dog]\n\n");

//     // puts("=== BATCH 3: Overlapping Deletes ===");
//     // markdown_delete(doc, 2, 0, 10); // Delete "The slow "
//     // markdown_insert(doc, 2, 0, "A fast "); // Add "A fast " in front
//     // markdown_delete(doc, 2, 17, 9); // Delete "smart dog"
//     // markdown_insert(doc, 2, 17, "clever cat");

//     // puts("Incrementing version to 3...");
//     // markdown_increment_version(doc);
//     // markdown_print(doc, stdout);
//     // printf("\n--- Expected v3: [A fast brown jumps over the clever cat]\n\n");

//     // markdown_free(doc);

//     document *doc = markdown_init();
//     markdown_insert(doc, 0, 0, "Hello World");
//     markdown_increment_version(doc);
//     //markdown_print(doc, stdout);

//     puts("hello");
//     markdown_delete(doc, 1, 5, 6);
//     markdown_increment_version(doc);
//     //markdown_print(doc, stdout);

//     free(doc);

//     return 0;
// }


