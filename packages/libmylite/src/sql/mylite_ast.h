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
    MYLITE_SQL_AST_CREATE_TABLE_STATEMENT = 13,
    MYLITE_SQL_AST_DROP_TABLE_STATEMENT = 14,
    MYLITE_SQL_AST_SHOW_TABLES_STATEMENT = 15,
    MYLITE_SQL_AST_COLUMN_DEFINITION_LIST = 16,
    MYLITE_SQL_AST_COLUMN_DEFINITION = 17,
    MYLITE_SQL_AST_INTEGER_TYPE = 18,
    MYLITE_SQL_AST_NULLABILITY = 19,
    MYLITE_SQL_AST_RENAME_TABLE_STATEMENT = 20,
    MYLITE_SQL_AST_INSERT_STATEMENT = 21,
    MYLITE_SQL_AST_IDENTIFIER_LIST = 22,
    MYLITE_SQL_AST_INSERT_ROW_LIST = 23,
    MYLITE_SQL_AST_INSERT_ROW = 24,
    MYLITE_SQL_AST_FROM_TABLE = 25,
    MYLITE_SQL_AST_WHERE_CLAUSE = 26,
    MYLITE_SQL_AST_COMPARISON_PREDICATE = 27,
    MYLITE_SQL_AST_IS_NULL_PREDICATE = 28,
    MYLITE_SQL_AST_ORDER_BY_CLAUSE = 29,
    MYLITE_SQL_AST_ORDER_DIRECTION = 30,
    MYLITE_SQL_AST_LIMIT_CLAUSE = 31,
    MYLITE_SQL_AST_DELETE_STATEMENT = 32,
    MYLITE_SQL_AST_UPDATE_STATEMENT = 33,
    MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST = 34,
    MYLITE_SQL_AST_UPDATE_ASSIGNMENT = 35,
    MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT = 36,
    MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT = 37,
    MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT = 38,
    MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT = 39,
    MYLITE_SQL_AST_DATABASE_FUNCTION = 40,
    MYLITE_SQL_AST_SCHEMA_FUNCTION = 41,
    MYLITE_SQL_AST_USER_FUNCTION = 42,
    MYLITE_SQL_AST_CURRENT_USER_FUNCTION = 43,
    MYLITE_SQL_AST_VERSION_FUNCTION = 44,
    MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR = 45,
    MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST = 46,
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
    MYLITE_SQL_AST_OPERATOR_EQUAL = 7,
    MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL = 8,
    MYLITE_SQL_AST_OPERATOR_NOT_EQUAL = 9,
    MYLITE_SQL_AST_OPERATOR_LESS = 10,
    MYLITE_SQL_AST_OPERATOR_LESS_EQUAL = 11,
    MYLITE_SQL_AST_OPERATOR_GREATER = 12,
    MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL = 13,
    MYLITE_SQL_AST_OPERATOR_IS_NULL = 14,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL = 15,
};

enum mylite_sql_ast_integer_type {
    MYLITE_SQL_AST_INTEGER_TYPE_NONE = 0,
    MYLITE_SQL_AST_INTEGER_TYPE_INT = 1,
    MYLITE_SQL_AST_INTEGER_TYPE_BIGINT = 2,
};

enum mylite_sql_ast_nullability {
    MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED = 0,
    MYLITE_SQL_AST_NULLABILITY_NULL = 1,
    MYLITE_SQL_AST_NULLABILITY_NOT_NULL = 2,
};

enum mylite_sql_ast_order_direction {
    MYLITE_SQL_AST_ORDER_DIRECTION_DEFAULT = 0,
    MYLITE_SQL_AST_ORDER_DIRECTION_ASC = 1,
    MYLITE_SQL_AST_ORDER_DIRECTION_DESC = 2,
};

struct mylite_sql_ast_literal_payload {
    enum mylite_sql_ast_literal_kind kind;
};

struct mylite_sql_ast_expression_payload {
    enum mylite_sql_ast_operator operator_kind;
};

struct mylite_sql_ast_integer_type_payload {
    enum mylite_sql_ast_integer_type kind;
    int is_unsigned;
};

struct mylite_sql_ast_nullability_payload {
    enum mylite_sql_ast_nullability kind;
};

struct mylite_sql_ast_order_direction_payload {
    enum mylite_sql_ast_order_direction kind;
};

union mylite_sql_ast_node_payload {
    struct mylite_sql_ast_literal_payload literal;
    struct mylite_sql_ast_expression_payload expression;
    struct mylite_sql_ast_integer_type_payload integer_type;
    struct mylite_sql_ast_nullability_payload nullability;
    struct mylite_sql_ast_order_direction_payload order_direction;
};

struct mylite_sql_ast_node {
    enum mylite_sql_ast_node_kind kind;
    struct mylite_sql_source_span span;
    union mylite_sql_ast_node_payload payload;
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

struct mylite_sql_ast_node *mylite_sql_ast_new_node(
    struct mylite_sql_ast *ast,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
);

void mylite_sql_ast_node_append_child(
    struct mylite_sql_ast_node *parent,
    struct mylite_sql_ast_node *child
);
void mylite_sql_ast_node_set_span(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_source_span span
);
void mylite_sql_ast_node_set_literal_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind literal_kind
);
void mylite_sql_ast_node_set_operator(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator operator_kind
);
void mylite_sql_ast_node_set_integer_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_integer_type_payload payload
);
void mylite_sql_ast_node_set_nullability(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability nullability
);
void mylite_sql_ast_node_set_order_direction(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction direction
);

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_literal_kind mylite_sql_ast_node_literal_kind(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_operator mylite_sql_ast_node_operator(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_integer_type mylite_sql_ast_node_integer_type(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_integer_type_is_unsigned(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_nullability mylite_sql_ast_node_nullability(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_order_direction mylite_sql_ast_node_order_direction(
    const struct mylite_sql_ast_node *node
);

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind);
const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind);
const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind);
const char *mylite_sql_ast_integer_type_name(enum mylite_sql_ast_integer_type integer_type);
const char *mylite_sql_ast_nullability_name(enum mylite_sql_ast_nullability nullability);
const char *mylite_sql_ast_order_direction_name(enum mylite_sql_ast_order_direction direction);

#endif
