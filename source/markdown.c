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

        if (cmd->start >= dr->start && cmd->end <= dr->end) {
            cmd->start = cmd->end = dr->end;
            return;
        }
        
        if (cmd->start >= dr->start && cmd->start < dr->end) {
            cmd->start = dr->end;
        }

        if (cmd->end > dr->start && cmd->end <= dr->end) {
            cmd->end = dr->start;
        }
    }
}

// === Versioning ===
void markdown_increment_version(document *doc) {
    if (!doc) return;

    DeletedRange** deleted_range_list = calloc(50, sizeof(void*));
    size_t dr_index = 0;

    time_sort(doc);

    size_t offset = 0;

    for (size_t i = 0; i < doc->commands_index; i++) {
        Command* cmd = doc->commands[i];
        if (!cmd) continue;

        switch (cmd->type) {
            case CMD_DELETE: {
                delete(doc, cmd->pos, cmd->len);
                DeletedRange* dr = malloc(sizeof(DeletedRange));
                dr->start = cmd->pos;
                dr->end = cmd->pos + cmd->len;
                deleted_range_list[dr_index++] = dr;
                if (dr_index % 50 == 0) {
                    deleted_range_list = realloc(deleted_range_list, 2 * dr_index * sizeof(void*));
                }
                break;
            }

            case CMD_INSERT: {
                cmd->pos = adjust_pos_for_deletions(cmd->pos, deleted_range_list, dr_index);
                cmd->pos += offset;
                insert(doc, cmd->pos, cmd->content);
                offset += strlen(cmd->content);
                break;
            }

            case CMD_NEWLINE:
            case CMD_HEADING:
            case CMD_QUOTE:
            case CMD_OLIST:
            case CMD_ULIST:
            case CMD_HRULE: {
                cmd->pos = adjust_pos_for_deletions(cmd->pos, deleted_range_list, dr_index);
                cmd->pos += offset;
                switch (cmd->type) {
                    case CMD_NEWLINE: insert_newline(doc, cmd->pos); offset += 1; break;
                    case CMD_HEADING: insert_heading(doc, cmd->level, cmd->pos); offset += (cmd->level + 1); break;
                    case CMD_QUOTE: insert_blockquote(doc, cmd->pos); offset += 2; break;
                    case CMD_OLIST: insert_ordered_list(doc, cmd->pos); offset += 3; break;
                    case CMD_ULIST: insert_unordered_list(doc, cmd->pos); offset += 2; break;
                    case CMD_HRULE: insert_horizontal_rule(doc, cmd->pos); offset += 4; break;
                    default: break;
                }
                break;
            }

            case CMD_BOLD:
            case CMD_ITALIC:
            case CMD_CODE: {
                adjust_range_for_deletions(cmd, deleted_range_list, dr_index);
                if (cmd->start >= cmd->end) break;

                cmd->start += offset;
                cmd->end += offset;

                const char* style = cmd->type == CMD_BOLD ? "**" :
                                    cmd->type == CMD_ITALIC ? "*" : "`";

                apply_inline(doc, cmd->start, cmd->end, style);
                offset += 2 * strlen(style);
                break;
            }

            case CMD_LINK: {
                adjust_range_for_deletions(cmd, deleted_range_list, dr_index);
                if (cmd->start >= cmd->end) break;

                cmd->start += offset;
                cmd->end += offset;

                insert_link(doc, cmd->start, cmd->end, cmd->content);
                offset += 2 + strlen(cmd->content) + 2; // [ + ]() = 4 + url len
                break;
            }

            default:
                fprintf(stderr, "Unknown command type\n");
                break;
        }

        if (cmd->content) free(cmd->content);
        free(cmd);
        doc->commands[i] = NULL;
    }

    for (size_t i = 0; i < dr_index; i++) free(deleted_range_list[i]);
    free(deleted_range_list);

    doc->version++;
}

