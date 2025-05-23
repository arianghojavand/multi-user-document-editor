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

int main(void) {
    document *doc = markdown_init();
    if (!doc) {
        fprintf(stderr, "Failed to init document\n");
        return 1;
    }

    uint64_t ver;
    size_t pos;
    char *buf;


    // —— Version 0: queue first 3 items ——
    ver = doc->version;
    printf("Before version %ld:\n", ver);
    // Item 1 at pos 0
    pos = 0;
    markdown_ordered_list(doc, ver, pos);

  
    // Item 2 at end of current (still empty) doc → pos 0
    buf = markdown_flatten(doc);
    pos = strlen(buf);
    free(buf);
    markdown_ordered_list(doc, ver, pos);

    // Item 3 at end of current doc
    buf = markdown_flatten(doc);
    pos = strlen(buf);
    free(buf);
    markdown_ordered_list(doc, ver, pos);

    
    // Commit to v1
    
    markdown_increment_version(doc);
   
    printf("After version: %ld\n", doc->version);

    markdown_print(doc, stdout);
    printf("\n---\n");

    
    // —— Version 1: queue last 2 items ——
    ver = doc->version;
    // Item 4
    buf = markdown_flatten(doc);
    pos = strlen(buf);
    free(buf);
    markdown_ordered_list(doc, ver, pos);

    // Item 5
    buf = markdown_flatten(doc);
    pos = strlen(buf);
    free(buf);
    markdown_ordered_list(doc, ver, pos);


    // Final commit to v2
    markdown_increment_version(doc);
    printf("After version %ld:\n", doc->version);
    markdown_print(doc, stdout);
    printf("\n");

    markdown_free(doc);
    return 0;
}


