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

    return enqueue_command(doc, CMD_OLIST, pos, 0, NULL, 0, 0, 0) == SUCCESS ? SUCCESS : -1;
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
                insert_ordered_list(doc, current_command->pos);
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
//     document *doc = markdown_init();

//     markdown_insert(doc, 0, 0, "Project Plan");
//     markdown_heading(doc, 0, 1, 0);
//     markdown_newline(doc, 0, 12);
//     markdown_insert(doc, 0, 13, "Draft version x completed by @team.");
//     markdown_delete(doc, 0, 20, 1);
//     markdown_insert(doc, 0, 20, "1.0");
//     markdown_bold(doc, 0, 6, 17);            // "version 1.0"
//     markdown_italic(doc, 0, 15, 17);         // ".0"
//     markdown_code(doc, 0, 29, 34);           // "@team"
//     markdown_newline(doc, 0, 35);
//     markdown_insert(doc, 0, 36, "Tasks");
//     markdown_heading(doc, 0, 2, 36);
//     markdown_newline(doc, 0, 41);
//     markdown_insert(doc, 0, 42, "Setup environment");
//     markdown_code(doc, 0, 42, 59);
//     markdown_unordered_list(doc, 0, 42);
//     markdown_newline(doc, 0, 61);
//     markdown_insert(doc, 0, 62, "Write tests");
//     markdown_bold(doc, 0, 62, 73);
//     markdown_unordered_list(doc, 0, 62);
//     markdown_newline(doc, 0, 75);
//     markdown_insert(doc, 0, 76, "Review code");
//     markdown_italic(doc, 0, 76, 87);
//     markdown_unordered_list(doc, 0, 76);
//     markdown_newline(doc, 0, 89);
//     markdown_insert(doc, 0, 90, "Deliver presentation");

//     markdown_ordered_list(doc, 0, 90);

//     //markdown_newline(doc, 0, 110);
//     markdown_insert(doc, 0, 110, "Collect feedback");

//     markdown_ordered_list(doc, 0, 110);

//     markdown_newline(doc, 0, 128);
//     markdown_horizontal_rule(doc, 0, 129);
//     markdown_insert(doc, 0, 134, "Note: See documentation");
//     markdown_link(doc, 0, 15, 27, "https://docs.example.com");
//     markdown_blockquote(doc, 0, 134);

//     markdown_increment_version(doc);
//     markdown_print(doc, stdout);

//     markdown_free(doc);
//     return 0;
// }


