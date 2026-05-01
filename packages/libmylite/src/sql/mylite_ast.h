#ifndef MYLITE_SQL_MYLITE_AST_H
#define MYLITE_SQL_MYLITE_AST_H

#include "mylite_source_span.h"

#include <stddef.h>

enum mylite_sql_ast_node_kind {
    MYLITE_SQL_AST_SCRIPT = 0,
    MYLITE_SQL_AST_SELECT_STATEMENT = 1,
    MYLITE_SQL_AST_USE_STATEMENT = 2,
    MYLITE_SQL_AST_SELECT_LIST = 3,
    MYLITE_SQL_AST_SELECT_ITEM = 4,
    MYLITE_SQL_AST_FROM_DUAL = 5,
    MYLITE_SQL_AST_IDENTIFIER = 6,
    MYLITE_SQL_AST_QUALIFIED_IDENTIFIER = 7,
    MYLITE_SQL_AST_WILDCARD = 8,
    MYLITE_SQL_AST_LITERAL = 9,
    MYLITE_SQL_AST_UNARY_EXPRESSION = 10,
    MYLITE_SQL_AST_BINARY_EXPRESSION = 11,
    MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION = 12,
};

enum mylite_sql_ast_literal_kind {
    MYLITE_SQL_AST_LITERAL_NONE = 0,
    MYLITE_SQL_AST_LITERAL_INTEGER = 1,
    MYLITE_SQL_AST_LITERAL_DECIMAL = 2,
    MYLITE_SQL_AST_LITERAL_FLOAT = 3,
    MYLITE_SQL_AST_LITERAL_STRING = 4,
    MYLITE_SQL_AST_LITERAL_NATIONAL_STRING = 5,
    MYLITE_SQL_AST_LITERAL_HEX = 6,
    MYLITE_SQL_AST_LITERAL_BIT = 7,
    MYLITE_SQL_AST_LITERAL_TRUE = 8,
    MYLITE_SQL_AST_LITERAL_FALSE = 9,
    MYLITE_SQL_AST_LITERAL_NULL = 10,
};

enum mylite_sql_ast_operator {
    MYLITE_SQL_AST_OPERATOR_NONE = 0,
    MYLITE_SQL_AST_OPERATOR_POSITIVE = 1,
    MYLITE_SQL_AST_OPERATOR_NEGATIVE = 2,
    MYLITE_SQL_AST_OPERATOR_ADD = 3,
    MYLITE_SQL_AST_OPERATOR_SUBTRACT = 4,
    MYLITE_SQL_AST_OPERATOR_MULTIPLY = 5,
    MYLITE_SQL_AST_OPERATOR_DIVIDE = 6,
};

struct mylite_sql_ast_node {
    enum mylite_sql_ast_node_kind kind;
    enum mylite_sql_ast_literal_kind literal_kind;
    enum mylite_sql_ast_operator operator_kind;
    struct mylite_sql_source_span span;
    struct mylite_sql_ast_node *first_child;
    struct mylite_sql_ast_node *last_child;
    struct mylite_sql_ast_node *next_sibling;
    struct mylite_sql_ast_node *next_allocated;
};

struct mylite_sql_ast {
    struct mylite_sql_ast_node *first_allocated;
};

void mylite_sql_ast_init(struct mylite_sql_ast *ast);
void mylite_sql_ast_deinit(struct mylite_sql_ast *ast);

struct mylite_sql_ast_node *mylite_sql_ast_new_node(struct mylite_sql_ast *ast,
                                                    enum mylite_sql_ast_node_kind kind,
                                                    struct mylite_sql_source_span span);

void mylite_sql_ast_node_append_child(struct mylite_sql_ast_node *parent,
                                      struct mylite_sql_ast_node *child);
void mylite_sql_ast_node_set_span(struct mylite_sql_ast_node *node,
                                  struct mylite_sql_source_span span);
void mylite_sql_ast_node_set_literal_kind(struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_literal_kind literal_kind);
void mylite_sql_ast_node_set_operator(struct mylite_sql_ast_node *node,
                                      enum mylite_sql_ast_operator operator_kind);

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node);

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind);
const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind);
const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind);

#endif
