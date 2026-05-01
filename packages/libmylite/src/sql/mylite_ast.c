#include "mylite_ast.h"

#include <stdlib.h>

void mylite_sql_ast_init(struct mylite_sql_ast *ast)
{
    if (ast == NULL) {
        return;
    }

    ast->first_allocated = NULL;
}

void mylite_sql_ast_deinit(struct mylite_sql_ast *ast)
{
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

struct mylite_sql_ast_node *mylite_sql_ast_new_node(struct mylite_sql_ast *ast,
                                                    enum mylite_sql_ast_node_kind kind,
                                                    struct mylite_sql_source_span span)
{
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

void mylite_sql_ast_node_append_child(struct mylite_sql_ast_node *parent,
                                      struct mylite_sql_ast_node *child)
{
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

void mylite_sql_ast_node_set_span(struct mylite_sql_ast_node *node,
                                  struct mylite_sql_source_span span)
{
    if (node == NULL) {
        return;
    }

    node->span = span;
}

void mylite_sql_ast_node_set_literal_kind(struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_literal_kind literal_kind)
{
    if (node == NULL) {
        return;
    }

    node->literal_kind = literal_kind;
}

void mylite_sql_ast_node_set_operator(struct mylite_sql_ast_node *node,
                                      enum mylite_sql_ast_operator operator_kind)
{
    if (node == NULL) {
        return;
    }

    node->operator_kind = operator_kind;
}

void mylite_sql_ast_node_set_schema_option(struct mylite_sql_ast_node *node,
                                           enum mylite_sql_ast_schema_option schema_option)
{
    if (node == NULL) {
        return;
    }

    node->schema_option = schema_option;
}

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node)
{
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

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind)
{
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
    case MYLITE_SQL_AST_FROM_TABLE:
        return "from_table";
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
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        return "create_schema_statement";
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        return "alter_schema_statement";
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        return "drop_schema_statement";
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
        return "show_schemas_statement";
    case MYLITE_SQL_AST_IF_EXISTS:
        return "if_exists";
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
        return "if_not_exists";
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
        return "schema_option_list";
    case MYLITE_SQL_AST_SCHEMA_OPTION:
        return "schema_option";
    }

    return "unknown";
}

const char *mylite_sql_ast_schema_option_name(enum mylite_sql_ast_schema_option schema_option)
{
    switch (schema_option) {
    case MYLITE_SQL_AST_SCHEMA_OPTION_NONE:
        return "none";
    case MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET:
        return "character_set";
    case MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE:
        return "collate";
    case MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION:
        return "encryption";
    case MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY:
        return "read_only";
    }

    return "unknown";
}

const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind)
{
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

const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind)
{
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
    }

    return "unknown";
}
