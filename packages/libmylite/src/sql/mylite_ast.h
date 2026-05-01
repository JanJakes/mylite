#ifndef MYLITE_SQL_MYLITE_AST_H
#define MYLITE_SQL_MYLITE_AST_H

#include "mylite_source_span.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_sql_ast_node_kind {
    MYLITE_SQL_AST_SCRIPT = 0,
    MYLITE_SQL_AST_SELECT_STATEMENT = 1,
    MYLITE_SQL_AST_USE_STATEMENT = 2,
    MYLITE_SQL_AST_SELECT_LIST = 3,
    MYLITE_SQL_AST_SELECT_ITEM = 4,
    MYLITE_SQL_AST_FROM_DUAL = 5,
    MYLITE_SQL_AST_FROM_TABLE = 6,
    MYLITE_SQL_AST_IDENTIFIER = 7,
    MYLITE_SQL_AST_QUALIFIED_IDENTIFIER = 8,
    MYLITE_SQL_AST_WILDCARD = 9,
    MYLITE_SQL_AST_LITERAL = 10,
    MYLITE_SQL_AST_UNARY_EXPRESSION = 11,
    MYLITE_SQL_AST_BINARY_EXPRESSION = 12,
    MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION = 13,
    MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT = 14,
    MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT = 15,
    MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT = 16,
    MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT = 17,
    MYLITE_SQL_AST_IF_EXISTS = 18,
    MYLITE_SQL_AST_IF_NOT_EXISTS = 19,
    MYLITE_SQL_AST_SCHEMA_OPTION_LIST = 20,
    MYLITE_SQL_AST_SCHEMA_OPTION = 21,
    MYLITE_SQL_AST_SET_NAMES_STATEMENT = 22,
    MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT = 23,
    MYLITE_SQL_AST_DEFAULT = 24,
    MYLITE_SQL_AST_CREATE_TABLE_STATEMENT = 25,
    MYLITE_SQL_AST_COLUMN_DEFINITION_LIST = 26,
    MYLITE_SQL_AST_COLUMN_DEFINITION = 27,
    MYLITE_SQL_AST_COLUMN_TYPE = 28,
    MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST = 29,
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

enum mylite_sql_ast_schema_option {
    MYLITE_SQL_AST_SCHEMA_OPTION_NONE = 0,
    MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET = 1,
    MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE = 2,
    MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION = 3,
    MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY = 4,
};

enum mylite_sql_ast_column_type {
    MYLITE_SQL_AST_COLUMN_TYPE_NONE = 0,
    MYLITE_SQL_AST_COLUMN_TYPE_TINYINT = 1,
    MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT = 2,
    MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT = 3,
    MYLITE_SQL_AST_COLUMN_TYPE_INT = 4,
    MYLITE_SQL_AST_COLUMN_TYPE_BIGINT = 5,
    MYLITE_SQL_AST_COLUMN_TYPE_BOOL = 6,
    MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN = 7,
    MYLITE_SQL_AST_COLUMN_TYPE_CHAR = 8,
    MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR = 9,
    MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT = 10,
    MYLITE_SQL_AST_COLUMN_TYPE_TEXT = 11,
    MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT = 12,
    MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT = 13,
    MYLITE_SQL_AST_COLUMN_TYPE_BINARY = 14,
    MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY = 15,
    MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB = 16,
    MYLITE_SQL_AST_COLUMN_TYPE_BLOB = 17,
    MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB = 18,
    MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB = 19,
    MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL = 20,
    MYLITE_SQL_AST_COLUMN_TYPE_FLOAT = 21,
    MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE = 22,
    MYLITE_SQL_AST_COLUMN_TYPE_DATE = 23,
    MYLITE_SQL_AST_COLUMN_TYPE_TIME = 24,
    MYLITE_SQL_AST_COLUMN_TYPE_DATETIME = 25,
    MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP = 26,
    MYLITE_SQL_AST_COLUMN_TYPE_YEAR = 27,
};

struct mylite_sql_ast_node {
    struct mylite_sql_ast_node *first_child;
    struct mylite_sql_ast_node *last_child;
    struct mylite_sql_ast_node *next_sibling;
    struct mylite_sql_ast_node *next_allocated;
    uint64_t column_length;
    uint64_t column_precision;
    uint64_t column_scale;
    struct mylite_sql_source_span span;
    struct mylite_sql_source_span column_character_set;
    struct mylite_sql_source_span column_collation;
    enum mylite_sql_ast_node_kind kind;
    enum mylite_sql_ast_literal_kind literal_kind;
    enum mylite_sql_ast_operator operator_kind;
    enum mylite_sql_ast_schema_option schema_option;
    enum mylite_sql_ast_column_type column_type;
    unsigned int column_display_width;
    bool column_type_unsigned;
    bool column_type_signed;
    bool has_column_display_width;
    bool has_column_length;
    bool has_column_precision;
    bool has_column_scale;
    bool has_column_character_set;
    bool has_column_collation;
    bool column_binary_attribute;
    bool column_byte_attribute;
    bool column_zerofill_attribute;
    bool column_national_attribute;
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
void mylite_sql_ast_node_set_schema_option(struct mylite_sql_ast_node *node,
                                           enum mylite_sql_ast_schema_option schema_option);
void mylite_sql_ast_node_set_column_type(struct mylite_sql_ast_node *node,
                                         enum mylite_sql_ast_column_type column_type);
void mylite_sql_ast_node_set_column_type_signed(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_type_unsigned(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_display_width(struct mylite_sql_ast_node *node,
                                                  unsigned int display_width);
void mylite_sql_ast_node_set_column_length(struct mylite_sql_ast_node *node, uint64_t length);
void mylite_sql_ast_node_set_column_precision(struct mylite_sql_ast_node *node, uint64_t precision);
void mylite_sql_ast_node_set_column_scale(struct mylite_sql_ast_node *node, uint64_t scale);
void mylite_sql_ast_node_set_column_character_set(struct mylite_sql_ast_node *node,
                                                  struct mylite_sql_source_span span);
void mylite_sql_ast_node_set_column_collation(struct mylite_sql_ast_node *node,
                                              struct mylite_sql_source_span span);
void mylite_sql_ast_node_set_column_binary_attribute(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_byte_attribute(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_zerofill_attribute(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_national_attribute(struct mylite_sql_ast_node *node);

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node);

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind);
const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind);
const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind);
const char *mylite_sql_ast_schema_option_name(enum mylite_sql_ast_schema_option schema_option);
const char *mylite_sql_ast_column_type_name(enum mylite_sql_ast_column_type column_type);

#endif
