#ifndef MARKDOWN_H
#define MARKDOWN_H
#include <stdio.h>
#include <stdint.h>
#include "document.h"  
#include <stdbool.h>
#include <stddef.h>
/**
 * The given file contains all the functions you will be required to complete. You are free to and encouraged to create
 * more helper functions to help assist you when creating the Document. For the automated marking you can expect unit tests
 * for the following tests, verifying if the Document functionalities are correctly implemented. All the commands are explained 
 * in detail in the assignment spec.
 */

// Return -1 if the cursor position is invalid

// Initialize and free a Document
Document * markdown_init(void);
void markdown_free(Document *doc);

// === Edit Commands ===
int markdown_insert(Document *doc, uint64_t version, size_t pos, const char *content);
int markdown_delete(Document *doc, uint64_t version, size_t pos, size_t len);

// === Formatting Commands ===
int markdown_newline(Document *doc, uint64_t version, size_t pos);
int markdown_heading(Document *doc, uint64_t version, size_t level, size_t pos);
int markdown_bold(Document *doc, uint64_t version, size_t start, size_t end);
int markdown_italic(Document *doc, uint64_t version, size_t start, size_t end);
int markdown_blockquote(Document *doc, uint64_t version, size_t pos);
int markdown_ordered_list(Document *doc, uint64_t version, size_t pos);
int markdown_unordered_list(Document *doc, uint64_t version, size_t pos);
int markdown_code(Document *doc, uint64_t version, size_t start, size_t end);
int markdown_horizontal_rule(Document *doc, uint64_t version, size_t pos);
int markdown_link(Document *doc, uint64_t version, size_t start, size_t end, const char *url);

// === Utilities ===
void markdown_print(const Document *doc, FILE *stream);
char *markdown_flatten(const Document *doc);

// === Versioning ===
void markdown_increment_version(Document *doc);

#endif // MARKDOWN_H
