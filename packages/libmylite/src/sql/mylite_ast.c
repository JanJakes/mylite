#include "mylite_ast.h"

#include <stdlib.h>

void mylite_sql_ast_init(struct mylite_sql_ast *ast) {
    if (ast == NULL) {
        return;
    }

    ast->first_allocated = NULL;
}

void mylite_sql_ast_deinit(struct mylite_sql_ast *ast) {
    struct mylite_sql_ast_node *node = NULL;

    if (ast == NULL) {
        return;
    }

    node = ast->first_allocated;
    while (node != NULL) {
        struct mylite_sql_ast_node *next = node->next_allocated;
        free(node);
        node = next;
    }
    ast->first_allocated = NULL;
}

struct mylite_sql_ast_node *mylite_sql_ast_new_node(
    struct mylite_sql_ast *ast,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
) {
    struct mylite_sql_ast_node *node = NULL;

    if (ast == NULL) {
        return NULL;
    }

    node = calloc(1U, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }

    node->kind = kind;
    node->span = span;
    node->next_allocated = ast->first_allocated;
    ast->first_allocated = node;
    return node;
}

void mylite_sql_ast_node_append_child(
    struct mylite_sql_ast_node *parent,
    struct mylite_sql_ast_node *child
) {
    if (parent == NULL || child == NULL) {
        return;
    }

    child->next_sibling = NULL;
    if (parent->last_child == NULL) {
        parent->first_child = child;
    } else {
        parent->last_child->next_sibling = child;
    }
    parent->last_child = child;
}

void mylite_sql_ast_node_set_span(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_source_span span
) {
    if (node == NULL) {
        return;
    }

    node->span = span;
}

void mylite_sql_ast_node_set_literal_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind literal_kind
) {
    if (node == NULL) {
        return;
    }

    node->payload.literal.kind = literal_kind;
}

void mylite_sql_ast_node_set_operator(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator operator_kind
) {
    if (node == NULL) {
        return;
    }

    node->payload.expression.operator_kind = operator_kind;
}

void mylite_sql_ast_node_set_integer_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_integer_type_payload payload
) {
    if (node == NULL) {
        return;
    }

    node->payload.integer_type = payload;
}

void mylite_sql_ast_node_set_nullability(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability nullability
) {
    if (node == NULL) {
        return;
    }

    node->payload.nullability.kind = nullability;
}

void mylite_sql_ast_node_set_order_direction(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction direction
) {
    if (node == NULL) {
        return;
    }

    node->payload.order_direction.kind = direction;
}

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node) {
    const struct mylite_sql_ast_node *child = NULL;
    size_t count = 0U;

    if (node == NULL) {
        return 0U;
    }

    child = node->first_child;
    while (child != NULL) {
        ++count;
        child = child->next_sibling;
    }
    return count;
}

enum mylite_sql_ast_literal_kind mylite_sql_ast_node_literal_kind(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_SQL_AST_LITERAL_NONE;
    }

    return node->payload.literal.kind;
}

enum mylite_sql_ast_operator mylite_sql_ast_node_operator(const struct mylite_sql_ast_node *node) {
    if (node == NULL || (node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION &&
                         node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION &&
                         node->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE &&
                         node->kind != MYLITE_SQL_AST_IS_NULL_PREDICATE)) {
        return MYLITE_SQL_AST_OPERATOR_NONE;
    }

    return node->payload.expression.operator_kind;
}

enum mylite_sql_ast_integer_type mylite_sql_ast_node_integer_type(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return MYLITE_SQL_AST_INTEGER_TYPE_NONE;
    }

    return node->payload.integer_type.kind;
}

int mylite_sql_ast_node_integer_type_is_unsigned(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_INTEGER_TYPE) {
        return 0;
    }

    return node->payload.integer_type.is_unsigned;
}

enum mylite_sql_ast_nullability mylite_sql_ast_node_nullability(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_NULLABILITY) {
        return MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED;
    }

    return node->payload.nullability.kind;
}

enum mylite_sql_ast_order_direction mylite_sql_ast_node_order_direction(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_ORDER_DIRECTION) {
        return MYLITE_SQL_AST_ORDER_DIRECTION_DEFAULT;
    }

    return node->payload.order_direction.kind;
}

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_SCRIPT:
        return "script";
    case MYLITE_SQL_AST_SELECT_STATEMENT:
        return "select_statement";
    case MYLITE_SQL_AST_USE_STATEMENT:
        return "use_statement";
    case MYLITE_SQL_AST_SELECT_LIST:
        return "select_list";
    case MYLITE_SQL_AST_SELECT_ITEM:
        return "select_item";
    case MYLITE_SQL_AST_FROM_DUAL:
        return "from_dual";
    case MYLITE_SQL_AST_IDENTIFIER:
        return "identifier";
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return "qualified_identifier";
    case MYLITE_SQL_AST_WILDCARD:
        return "wildcard";
    case MYLITE_SQL_AST_LITERAL:
        return "literal";
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return "unary_expression";
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return "binary_expression";
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return "parenthesized_expression";
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return "create_table_statement";
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return "drop_table_statement";
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
        return "show_tables_statement";
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        return "column_definition_list";
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
        return "column_definition";
    case MYLITE_SQL_AST_INTEGER_TYPE:
        return "integer_type";
    case MYLITE_SQL_AST_NULLABILITY:
        return "nullability";
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
        return "rename_table_statement";
    case MYLITE_SQL_AST_INSERT_STATEMENT:
        return "insert_statement";
    case MYLITE_SQL_AST_IDENTIFIER_LIST:
        return "identifier_list";
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
        return "insert_row_list";
    case MYLITE_SQL_AST_INSERT_ROW:
        return "insert_row";
    case MYLITE_SQL_AST_FROM_TABLE:
        return "from_table";
    case MYLITE_SQL_AST_WHERE_CLAUSE:
        return "where_clause";
    case MYLITE_SQL_AST_COMPARISON_PREDICATE:
        return "comparison_predicate";
    case MYLITE_SQL_AST_IS_NULL_PREDICATE:
        return "is_null_predicate";
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        return "order_by_clause";
    case MYLITE_SQL_AST_ORDER_DIRECTION:
        return "order_direction";
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
        return "limit_clause";
    }

    return "unknown";
}

const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_LITERAL_NONE:
        return "none";
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return "integer";
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
        return "decimal";
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return "float";
    case MYLITE_SQL_AST_LITERAL_STRING:
        return "string";
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        return "national_string";
    case MYLITE_SQL_AST_LITERAL_HEX:
        return "hex";
    case MYLITE_SQL_AST_LITERAL_BIT:
        return "bit";
    case MYLITE_SQL_AST_LITERAL_TRUE:
        return "true";
    case MYLITE_SQL_AST_LITERAL_FALSE:
        return "false";
    case MYLITE_SQL_AST_LITERAL_NULL:
        return "null";
    }

    return "unknown";
}

const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_NONE:
        return "none";
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        return "positive";
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        return "negative";
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "add";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "subtract";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "multiply";
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        return "divide";
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "equal";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "null_safe_equal";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "not_equal";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "less";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "less_equal";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return "greater";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return "greater_equal";
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return "is_null";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        return "is_not_null";
    }

    return "unknown";
}

const char *mylite_sql_ast_integer_type_name(enum mylite_sql_ast_integer_type integer_type) {
    switch (integer_type) {
    case MYLITE_SQL_AST_INTEGER_TYPE_NONE:
        return "none";
    case MYLITE_SQL_AST_INTEGER_TYPE_INT:
        return "int";
    case MYLITE_SQL_AST_INTEGER_TYPE_BIGINT:
        return "bigint";
    }

    return "unknown";
}

const char *mylite_sql_ast_nullability_name(enum mylite_sql_ast_nullability nullability) {
    switch (nullability) {
    case MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED:
        return "unspecified";
    case MYLITE_SQL_AST_NULLABILITY_NULL:
        return "null";
    case MYLITE_SQL_AST_NULLABILITY_NOT_NULL:
        return "not_null";
    }

    return "unknown";
}

const char *mylite_sql_ast_order_direction_name(enum mylite_sql_ast_order_direction direction) {
    switch (direction) {
    case MYLITE_SQL_AST_ORDER_DIRECTION_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_ORDER_DIRECTION_ASC:
        return "asc";
    case MYLITE_SQL_AST_ORDER_DIRECTION_DESC:
        return "desc";
    }

    return "unknown";
}
