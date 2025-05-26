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

size_t adjust_pos_for_deletions(size_t pos, DeletedRange** drs, size_t dr_count) {
    for (size_t i = 0; i < dr_count; i++) {
        if (pos >= drs[i]->start && pos < drs[i]->end) {
            return drs[i]->start;
        }
    }
    return pos;
}

void adjust_range_for_deletions(Command* cmd, DeletedRange** drs, size_t dr_count) {
    for (size_t i = 0; i < dr_count; i++) {
        DeletedRange* dr = drs[i];

        //if entire range deleted fuck outta here
        if (cmd->start >= dr->start && cmd->end <= dr->end) {
            cmd->start = cmd->end = dr->end;
            return;
        }

        //is start is in deleted snap to closest
        if (cmd->start >= dr->start && cmd->start < dr->end) {
            size_t dist_to_start = cmd->start - dr->start;
            size_t dist_to_end = dr->end - cmd->start;
            cmd->start = (dist_to_start <= dist_to_end) ? dr->start : dr->end;
        }

        //if end is in deleted snap to closest
        if (cmd->end > dr->start && cmd->end <= dr->end) {
            size_t dist_to_start = cmd->end - dr->start;
            size_t dist_to_end = dr->end - cmd->end;
            cmd->end = (dist_to_start <= dist_to_end) ? dr->start : dr->end;
        }
    }
}

// === Versioning ===
void markdown_increment_version(document *doc) {
    
   //(1) go iteratively through cmds (from head to tail)

    DeletedRange** deleted_range_list = calloc(50, sizeof(void*));
    size_t dr_index = 0;

    //sort it up by timestamp
    time_sort(doc);
    
    
    for (size_t i = 0; i < doc->commands_index; i++) {
        Command* current_command = doc->commands[i];

        if (!current_command) continue;
        
        switch (current_command->type) {
            case CMD_INSERT:
                
                current_command->pos = adjust_pos_for_deletions(current_command->pos, deleted_range_list, dr_index);
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
                current_command->pos = adjust_pos_for_deletions(current_command->pos, deleted_range_list, dr_index);
                insert_newline(doc, current_command->pos);
                break;

            case CMD_HEADING:
                current_command->pos = adjust_pos_for_deletions(current_command->pos, deleted_range_list, dr_index);
                insert_heading(doc, current_command->level, current_command->pos);
                break;

            case CMD_BOLD:
                adjust_range_for_deletions(current_command, deleted_range_list, dr_index);
                if (current_command->start >= current_command->end) continue;
                insert_bold(doc, current_command->start, current_command->end);
                break;
            
            case CMD_ITALIC: 
                adjust_range_for_deletions(current_command, deleted_range_list, dr_index);
                if (current_command->start >= current_command->end) continue;
                insert_italic(doc, current_command->start, current_command->end);
                break;
            
            case CMD_QUOTE:
                current_command->pos = adjust_pos_for_deletions(current_command->pos, deleted_range_list, dr_index);
                insert_blockquote(doc, current_command->pos);
                break;

            case CMD_OLIST:
                current_command->pos = adjust_pos_for_deletions(current_command->pos, deleted_range_list, dr_index);
                insert_ordered_list(doc, current_command->pos);
                break;

            case CMD_ULIST:
                current_command->pos = adjust_pos_for_deletions(current_command->pos, deleted_range_list, dr_index);
                insert_unordered_list(doc, current_command->pos);
                break;

            case CMD_CODE: 
                adjust_range_for_deletions(current_command, deleted_range_list, dr_index);
                if (current_command->start >= current_command->end) continue;
                insert_code(doc, current_command->start, current_command->end);
                break;

            case CMD_HRULE:
                current_command->pos = adjust_pos_for_deletions(current_command->pos, deleted_range_list, dr_index);
                insert_horizontal_rule(doc, current_command->pos);
                break;

            case CMD_LINK: 
                adjust_range_for_deletions(current_command, deleted_range_list, dr_index);
                if (current_command->start >= current_command->end) continue;
                insert_link(doc, current_command->start, current_command->end, current_command->content);
                break;
            
            default: //command not recognised
                fprintf(stderr, "markdown_inc_ver: command not recognisded\n");
                break;

        }


        Command* temp = current_command;

        if (temp->content) {
            free(temp->content);
        }
        
        free(temp);
        doc->commands[i] = NULL;

    }

    for (size_t i = 0; i < dr_index; i++) {
        if (!deleted_range_list[i]) continue;

        free(deleted_range_list[i]);
    }

    free(deleted_range_list);
    
    if (doc) doc->version++;

    
}

