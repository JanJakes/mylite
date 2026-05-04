#ifndef MYLITE_RUNTIME_MYLITE_SPAN_H
#define MYLITE_RUNTIME_MYLITE_SPAN_H

#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>

bool mylite_span_equal_ci(struct mylite_sql_source_span span, const char *text);
bool mylite_ascii_case_equal(const char *left, const char *right);
void mylite_uppercase_ascii_text(char *text);
char *mylite_copy_schema_text_span(const struct mylite_sql_ast_node *node);
char *mylite_copy_identifier_span(const struct mylite_sql_ast_node *node);
int mylite_copy_identifier_parts(const struct mylite_sql_ast_node *identifier, char **parts,
                                 size_t *part_count);
char *mylite_copy_string_literal_span(const struct mylite_sql_ast_node *node);
char *mylite_copy_unquoted_span_text(struct mylite_sql_source_span span);
char *mylite_copy_nonempty_cstring(const char *text);
char *mylite_copy_span_text(const char *text, size_t length);
bool mylite_span_contains_newline(const char *text, size_t length);
bool mylite_text_contains_word(const char *text, const char *word);
bool mylite_column_default_is_current_timestamp(const char *default_text);
const struct mylite_sql_ast_node *mylite_ast_child_at(const struct mylite_sql_ast_node *node,
                                                      size_t index);
const struct mylite_sql_ast_node *mylite_ast_find_child_kind(const struct mylite_sql_ast_node *node,
                                                             enum mylite_sql_ast_node_kind kind);
const struct mylite_sql_ast_node *
mylite_ast_single_statement(const struct mylite_sql_ast_node *root);

#endif
