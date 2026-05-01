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

void mylite_sql_ast_node_set_column_type(struct mylite_sql_ast_node *node,
                                         enum mylite_sql_ast_column_type column_type)
{
    if (node == NULL) {
        return;
    }

    node->column_type = column_type;
}

void mylite_sql_ast_node_set_column_type_signed(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_type_signed = true;
}

void mylite_sql_ast_node_set_column_type_unsigned(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_type_unsigned = true;
}

void mylite_sql_ast_node_set_column_display_width(struct mylite_sql_ast_node *node,
                                                  unsigned int display_width)
{
    if (node == NULL) {
        return;
    }

    node->has_column_display_width = true;
    node->column_display_width = display_width;
}

void mylite_sql_ast_node_set_column_length(struct mylite_sql_ast_node *node, uint64_t length)
{
    if (node == NULL) {
        return;
    }

    node->has_column_length = true;
    node->column_length = length;
}

void mylite_sql_ast_node_set_column_character_set(struct mylite_sql_ast_node *node,
                                                  struct mylite_sql_source_span span)
{
    if (node == NULL) {
        return;
    }

    node->has_column_character_set = true;
    node->column_character_set = span;
}

void mylite_sql_ast_node_set_column_collation(struct mylite_sql_ast_node *node,
                                              struct mylite_sql_source_span span)
{
    if (node == NULL) {
        return;
    }

    node->has_column_collation = true;
    node->column_collation = span;
}

void mylite_sql_ast_node_set_column_binary_attribute(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_binary_attribute = true;
}

void mylite_sql_ast_node_set_column_byte_attribute(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_byte_attribute = true;
}

void mylite_sql_ast_node_set_column_national_attribute(struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return;
    }

    node->column_national_attribute = true;
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
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        return "set_names_statement";
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
        return "set_character_set_statement";
    case MYLITE_SQL_AST_DEFAULT:
        return "default";
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return "create_table_statement";
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        return "column_definition_list";
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
        return "column_definition";
    case MYLITE_SQL_AST_COLUMN_TYPE:
        return "column_type";
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
        return "column_type_attribute_list";
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

const char *mylite_sql_ast_column_type_name(enum mylite_sql_ast_column_type column_type)
{
    switch (column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
        return "none";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
        return "tinyint";
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
        return "smallint";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
        return "mediumint";
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
        return "int";
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        return "bigint";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
        return "bool";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
        return "boolean";
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
        return "char";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
        return "varchar";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
        return "tinytext";
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
        return "text";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
        return "mediumtext";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
        return "longtext";
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        return "binary";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
        return "varbinary";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
        return "tinyblob";
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
        return "blob";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
        return "mediumblob";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
        return "longblob";
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
