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
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST = 30,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE = 31,
    MYLITE_SQL_AST_CURRENT_TIMESTAMP = 32,
    MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT = 33,
    MYLITE_SQL_AST_KEY_PART_LIST = 34,
    MYLITE_SQL_AST_KEY_PART = 35,
    MYLITE_SQL_AST_INDEX_TYPE = 36,
    MYLITE_SQL_AST_INDEX_OPTION_LIST = 37,
    MYLITE_SQL_AST_INDEX_OPTION = 38,
    MYLITE_SQL_AST_SECONDARY_INDEX = 39,
    MYLITE_SQL_AST_UNIQUE_INDEX = 40,
    MYLITE_SQL_AST_TABLE_OPTION_LIST = 41,
    MYLITE_SQL_AST_TABLE_OPTION = 42,
    MYLITE_SQL_AST_DROP_TABLE_STATEMENT = 43,
    MYLITE_SQL_AST_TABLE_NAME_LIST = 44,
    MYLITE_SQL_AST_INSERT_VALUES_STATEMENT = 45,
    MYLITE_SQL_AST_INSERT_COLUMN_LIST = 46,
    MYLITE_SQL_AST_INSERT_ROW_LIST = 47,
    MYLITE_SQL_AST_INSERT_ROW = 48,
    MYLITE_SQL_AST_INSERT_VALUE_LIST = 49,
    MYLITE_SQL_AST_INSERT_SET_STATEMENT = 50,
    MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST = 51,
    MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT = 52,
    MYLITE_SQL_AST_TERNARY_EXPRESSION = 53,
    MYLITE_SQL_AST_EXPRESSION_LIST = 54,
    MYLITE_SQL_AST_WHERE_CLAUSE = 55,
    MYLITE_SQL_AST_ORDER_BY_CLAUSE = 56,
    MYLITE_SQL_AST_ORDER_ITEM_LIST = 57,
    MYLITE_SQL_AST_ORDER_ITEM = 58,
    MYLITE_SQL_AST_LIMIT_CLAUSE = 59,
    MYLITE_SQL_AST_LIMIT_BOUND = 60,
    MYLITE_SQL_AST_UPDATE_STATEMENT = 61,
    MYLITE_SQL_AST_UPDATE_TARGET = 62,
    MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST = 63,
    MYLITE_SQL_AST_UPDATE_ASSIGNMENT = 64,
    MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE = 65,
    MYLITE_SQL_AST_DELETE_STATEMENT = 66,
    MYLITE_SQL_AST_DELETE_TARGET = 67,
    MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE = 68,
    MYLITE_SQL_AST_START_TRANSACTION_STATEMENT = 69,
    MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT = 70,
    MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST = 71,
    MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC = 72,
    MYLITE_SQL_AST_COMMIT_STATEMENT = 73,
    MYLITE_SQL_AST_ROLLBACK_STATEMENT = 74,
    MYLITE_SQL_AST_TRANSACTION_COMPLETION = 75,
    MYLITE_SQL_AST_SAVEPOINT_STATEMENT = 76,
    MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT = 77,
    MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT = 78,
    MYLITE_SQL_AST_FUNCTION_CALL = 79,
    MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST = 80,
    MYLITE_SQL_AST_CASE_EXPRESSION = 81,
    MYLITE_SQL_AST_CASE_WHEN_LIST = 82,
    MYLITE_SQL_AST_CASE_WHEN = 83,
    MYLITE_SQL_AST_CAST_EXPRESSION = 84,
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
    MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT = 14,
    MYLITE_SQL_AST_OPERATOR_LOGICAL_AND = 15,
    MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR = 16,
    MYLITE_SQL_AST_OPERATOR_LOGICAL_OR = 17,
    MYLITE_SQL_AST_OPERATOR_BITWISE_NOT = 18,
    MYLITE_SQL_AST_OPERATOR_BITWISE_AND = 19,
    MYLITE_SQL_AST_OPERATOR_BITWISE_XOR = 20,
    MYLITE_SQL_AST_OPERATOR_BITWISE_OR = 21,
    MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT = 22,
    MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT = 23,
    MYLITE_SQL_AST_OPERATOR_IS_NULL = 24,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL = 25,
    MYLITE_SQL_AST_OPERATOR_IS_TRUE = 26,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE = 27,
    MYLITE_SQL_AST_OPERATOR_IS_FALSE = 28,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE = 29,
    MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN = 30,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN = 31,
    MYLITE_SQL_AST_OPERATOR_BETWEEN = 32,
    MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN = 33,
    MYLITE_SQL_AST_OPERATOR_LIKE = 34,
    MYLITE_SQL_AST_OPERATOR_NOT_LIKE = 35,
    MYLITE_SQL_AST_OPERATOR_IN = 36,
    MYLITE_SQL_AST_OPERATOR_NOT_IN = 37,
    MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE = 38,
    MYLITE_SQL_AST_OPERATOR_MODULO = 39,
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

enum mylite_sql_ast_column_attribute {
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NONE = 0,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL = 1,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL = 2,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT = 3,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_ON_UPDATE = 4,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT = 5,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE = 6,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE = 7,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COLUMN_FORMAT = 8,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_STORAGE = 9,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT = 10,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY = 11,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY = 12,
};

enum mylite_sql_ast_column_format {
    MYLITE_SQL_AST_COLUMN_FORMAT_NONE = 0,
    MYLITE_SQL_AST_COLUMN_FORMAT_DEFAULT = 1,
    MYLITE_SQL_AST_COLUMN_FORMAT_FIXED = 2,
    MYLITE_SQL_AST_COLUMN_FORMAT_DYNAMIC = 3,
};

enum mylite_sql_ast_column_storage {
    MYLITE_SQL_AST_COLUMN_STORAGE_NONE = 0,
    MYLITE_SQL_AST_COLUMN_STORAGE_DEFAULT = 1,
    MYLITE_SQL_AST_COLUMN_STORAGE_DISK = 2,
    MYLITE_SQL_AST_COLUMN_STORAGE_MEMORY = 3,
};

enum mylite_sql_ast_key_part_order {
    MYLITE_SQL_AST_KEY_PART_ORDER_NONE = 0,
    MYLITE_SQL_AST_KEY_PART_ORDER_ASC = 1,
    MYLITE_SQL_AST_KEY_PART_ORDER_DESC = 2,
};

enum mylite_sql_ast_index_algorithm {
    MYLITE_SQL_AST_INDEX_ALGORITHM_NONE = 0,
    MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE = 1,
    MYLITE_SQL_AST_INDEX_ALGORITHM_HASH = 2,
};

enum mylite_sql_ast_index_option {
    MYLITE_SQL_AST_INDEX_OPTION_NONE = 0,
    MYLITE_SQL_AST_INDEX_OPTION_USING = 1,
    MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE = 2,
    MYLITE_SQL_AST_INDEX_OPTION_COMMENT = 3,
    MYLITE_SQL_AST_INDEX_OPTION_VISIBLE = 4,
    MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE = 5,
    MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE = 6,
    MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE = 7,
};

enum mylite_sql_ast_table_option {
    MYLITE_SQL_AST_TABLE_OPTION_NONE = 0,
    MYLITE_SQL_AST_TABLE_OPTION_ENGINE = 1,
    MYLITE_SQL_AST_TABLE_OPTION_CHARACTER_SET = 2,
    MYLITE_SQL_AST_TABLE_OPTION_COLLATE = 3,
    MYLITE_SQL_AST_TABLE_OPTION_COMMENT = 4,
    MYLITE_SQL_AST_TABLE_OPTION_AUTO_INCREMENT = 5,
};

enum mylite_sql_ast_transaction_access_mode {
    MYLITE_SQL_AST_TRANSACTION_ACCESS_NONE = 0,
    MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE = 1,
    MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY = 2,
};

enum mylite_sql_ast_transaction_chain {
    MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT = 0,
    MYLITE_SQL_AST_TRANSACTION_CHAIN_YES = 1,
    MYLITE_SQL_AST_TRANSACTION_CHAIN_NO = 2,
};

enum mylite_sql_ast_transaction_release {
    MYLITE_SQL_AST_TRANSACTION_RELEASE_DEFAULT = 0,
    MYLITE_SQL_AST_TRANSACTION_RELEASE_YES = 1,
    MYLITE_SQL_AST_TRANSACTION_RELEASE_NO = 2,
};

struct mylite_sql_ast_node {
    struct mylite_sql_ast_node *first_child;
    struct mylite_sql_ast_node *last_child;
    struct mylite_sql_ast_node *next_sibling;
    struct mylite_sql_ast_node *next_allocated;
    uint64_t column_length;
    uint64_t column_precision;
    uint64_t column_scale;
    uint64_t limit_bound_value;
    struct mylite_sql_source_span span;
    struct mylite_sql_source_span column_character_set;
    struct mylite_sql_source_span column_collation;
    enum mylite_sql_ast_node_kind kind;
    enum mylite_sql_ast_literal_kind literal_kind;
    enum mylite_sql_ast_operator operator_kind;
    enum mylite_sql_ast_schema_option schema_option;
    enum mylite_sql_ast_column_type column_type;
    enum mylite_sql_ast_column_attribute column_attribute;
    enum mylite_sql_ast_column_format column_format;
    enum mylite_sql_ast_column_storage column_storage;
    enum mylite_sql_ast_key_part_order key_part_order;
    enum mylite_sql_ast_index_algorithm index_algorithm;
    enum mylite_sql_ast_index_option index_option;
    enum mylite_sql_ast_table_option table_option;
    enum mylite_sql_ast_transaction_access_mode transaction_access_mode;
    enum mylite_sql_ast_transaction_chain transaction_chain;
    enum mylite_sql_ast_transaction_release transaction_release;
    unsigned int column_display_width;
    bool column_type_unsigned;
    bool column_type_signed;
    bool has_column_display_width;
    bool has_column_length;
    bool has_column_precision;
    bool has_column_scale;
    bool has_limit_bound_value;
    bool has_column_character_set;
    bool has_column_collation;
    bool column_binary_attribute;
    bool column_byte_attribute;
    bool column_zerofill_attribute;
    bool column_national_attribute;
    bool case_expression_simple;
    bool drop_table_temporary;
    bool drop_table_restrict;
    bool drop_table_cascade;
    bool transaction_consistent_snapshot;
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
void mylite_sql_ast_node_set_column_attribute(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_column_attribute column_attribute);
void mylite_sql_ast_node_set_column_format(struct mylite_sql_ast_node *node,
                                           enum mylite_sql_ast_column_format column_format);
void mylite_sql_ast_node_set_column_storage(struct mylite_sql_ast_node *node,
                                            enum mylite_sql_ast_column_storage column_storage);
void mylite_sql_ast_node_set_key_part_order(struct mylite_sql_ast_node *node,
                                            enum mylite_sql_ast_key_part_order order);
void mylite_sql_ast_node_set_limit_bound_value(struct mylite_sql_ast_node *node, uint64_t value);
void mylite_sql_ast_node_set_index_algorithm(struct mylite_sql_ast_node *node,
                                             enum mylite_sql_ast_index_algorithm algorithm);
void mylite_sql_ast_node_set_index_option(struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_index_option option);
void mylite_sql_ast_node_set_table_option(struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_table_option option);
void mylite_sql_ast_node_set_drop_table_temporary(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_drop_table_restrict(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_drop_table_cascade(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_transaction_access_mode(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_transaction_access_mode access_mode);
void mylite_sql_ast_node_set_transaction_consistent_snapshot(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_transaction_completion(
    struct mylite_sql_ast_node *node, enum mylite_sql_ast_transaction_chain chain,
    enum mylite_sql_ast_transaction_release release);
void mylite_sql_ast_node_set_case_expression_simple(struct mylite_sql_ast_node *node);

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node);

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind);
const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind);
const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind);
const char *mylite_sql_ast_schema_option_name(enum mylite_sql_ast_schema_option schema_option);
const char *mylite_sql_ast_column_type_name(enum mylite_sql_ast_column_type column_type);
const char *
mylite_sql_ast_column_attribute_name(enum mylite_sql_ast_column_attribute column_attribute);
const char *mylite_sql_ast_column_format_name(enum mylite_sql_ast_column_format column_format);
const char *mylite_sql_ast_column_storage_name(enum mylite_sql_ast_column_storage column_storage);
const char *mylite_sql_ast_key_part_order_name(enum mylite_sql_ast_key_part_order order);
const char *mylite_sql_ast_index_algorithm_name(enum mylite_sql_ast_index_algorithm algorithm);
const char *mylite_sql_ast_index_option_name(enum mylite_sql_ast_index_option option);

#endif
