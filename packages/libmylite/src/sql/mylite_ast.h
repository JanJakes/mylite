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
    MYLITE_SQL_AST_GROUP_BY_CLAUSE = 85,
    MYLITE_SQL_AST_GROUP_ITEM_LIST = 86,
    MYLITE_SQL_AST_GROUP_ITEM = 87,
    MYLITE_SQL_AST_HAVING_CLAUSE = 88,
    MYLITE_SQL_AST_AGGREGATE_CALL = 89,
    MYLITE_SQL_AST_FROM_TABLE_REFERENCES = 90,
    MYLITE_SQL_AST_TABLE_REFERENCE_LIST = 91,
    MYLITE_SQL_AST_JOIN_EXPRESSION = 92,
    MYLITE_SQL_AST_JOIN_CONDITION = 93,
    MYLITE_SQL_AST_USING_COLUMN_LIST = 94,
    MYLITE_SQL_AST_USING_COLUMN = 95,
    MYLITE_SQL_AST_SUBQUERY_EXPRESSION = 96,
    MYLITE_SQL_AST_EXISTS_EXPRESSION = 97,
    MYLITE_SQL_AST_QUANTIFIED_COMPARISON = 98,
    MYLITE_SQL_AST_ROW_CONSTRUCTOR = 99,
    MYLITE_SQL_AST_QUERY_EXPRESSION = 100,
    MYLITE_SQL_AST_UNION_EXPRESSION = 101,
    MYLITE_SQL_AST_QUERY_PRIMARY = 102,
    MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE = 103,
    MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST = 104,
    MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT = 105,
    MYLITE_SQL_AST_INSERT_ROW_ALIAS = 106,
    MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST = 107,
    MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT = 108,
    MYLITE_SQL_AST_REPLACE_SET_STATEMENT = 109,
    MYLITE_SQL_AST_CREATE_INDEX_STATEMENT = 110,
    MYLITE_SQL_AST_DROP_INDEX_STATEMENT = 111,
    MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST = 112,
    MYLITE_SQL_AST_DDL_TABLE_OPTION = 113,
    MYLITE_SQL_AST_ALTER_TABLE_STATEMENT = 114,
    MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST = 115,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION = 116,
    MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION = 117,
    MYLITE_SQL_AST_RENAME_TABLE_STATEMENT = 118,
    MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST = 119,
    MYLITE_SQL_AST_RENAME_TABLE_PAIR = 120,
    MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT = 121,
    MYLITE_SQL_AST_SHOW_TABLES_STATEMENT = 122,
    MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT = 123,
    MYLITE_SQL_AST_SHOW_INDEX_STATEMENT = 124,
    MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT = 125,
    MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT = 126,
    MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT = 127,
    MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT = 128,
    MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT = 129,
    MYLITE_SQL_AST_SHOW_STATUS_STATEMENT = 130,
    MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT = 131,
    MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT = 132,
    MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT = 133,
    MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT = 134,
    MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT = 135,
    MYLITE_SQL_AST_DELETE_TARGET_LIST = 136,
    MYLITE_SQL_AST_DELETE_TARGET_NAME = 137,
    MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT = 138,
    MYLITE_SQL_AST_SET_USER_VARIABLE_STATEMENT = 139,
    MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_LIST = 140,
    MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT = 141,
    MYLITE_SQL_AST_PREPARE_STATEMENT = 142,
    MYLITE_SQL_AST_EXECUTE_STATEMENT = 143,
    MYLITE_SQL_AST_EXECUTE_USING_LIST = 144,
    MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT = 145,
    MYLITE_SQL_AST_PLACEHOLDER_STATEMENT = 146,
    MYLITE_SQL_AST_VALUES_STATEMENT = 147,
    MYLITE_SQL_AST_WINDOW_FUNCTION_CALL = 148,
    MYLITE_SQL_AST_OVER_CLAUSE = 149,
    MYLITE_SQL_AST_WINDOW_SPECIFICATION = 150,
    MYLITE_SQL_AST_WINDOW_CLAUSE = 151,
    MYLITE_SQL_AST_WINDOW_DEFINITION_LIST = 152,
    MYLITE_SQL_AST_WINDOW_DEFINITION = 153,
    MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE = 154,
    MYLITE_SQL_AST_WINDOW_FRAME_CLAUSE = 155,
    MYLITE_SQL_AST_WINDOW_FRAME_BOUND = 156,
    MYLITE_SQL_AST_WINDOW_NULL_TREATMENT = 157,
};

enum mylite_sql_ast_placeholder_statement_kind {
    MYLITE_SQL_AST_PLACEHOLDER_CALL = 0,
    MYLITE_SQL_AST_PLACEHOLDER_CREATE_PROCEDURE = 1,
    MYLITE_SQL_AST_PLACEHOLDER_CREATE_FUNCTION = 2,
    MYLITE_SQL_AST_PLACEHOLDER_CREATE_TRIGGER = 3,
    MYLITE_SQL_AST_PLACEHOLDER_CREATE_EVENT = 4,
    MYLITE_SQL_AST_PLACEHOLDER_DROP_PROCEDURE = 5,
    MYLITE_SQL_AST_PLACEHOLDER_DROP_FUNCTION = 6,
    MYLITE_SQL_AST_PLACEHOLDER_DROP_TRIGGER = 7,
    MYLITE_SQL_AST_PLACEHOLDER_DROP_EVENT = 8,
    MYLITE_SQL_AST_PLACEHOLDER_SIGNAL = 9,
    MYLITE_SQL_AST_PLACEHOLDER_EXPLAIN = 10,
    MYLITE_SQL_AST_PLACEHOLDER_ALTER_USER = 11,
    MYLITE_SQL_AST_PLACEHOLDER_CREATE_USER = 12,
    MYLITE_SQL_AST_PLACEHOLDER_CREATE_ROLE = 13,
    MYLITE_SQL_AST_PLACEHOLDER_DROP_USER = 14,
    MYLITE_SQL_AST_PLACEHOLDER_DROP_ROLE = 15,
    MYLITE_SQL_AST_PLACEHOLDER_GRANT = 16,
    MYLITE_SQL_AST_PLACEHOLDER_RENAME_USER = 17,
    MYLITE_SQL_AST_PLACEHOLDER_REVOKE = 18,
    MYLITE_SQL_AST_PLACEHOLDER_SET_DEFAULT_ROLE = 19,
    MYLITE_SQL_AST_PLACEHOLDER_SET_PASSWORD = 20,
    MYLITE_SQL_AST_PLACEHOLDER_SET_ROLE = 21,
    MYLITE_SQL_AST_PLACEHOLDER_SHOW_GRANTS = 22,
    MYLITE_SQL_AST_PLACEHOLDER_SHOW_PRIVILEGES = 23,
    MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING = 24,
    MYLITE_SQL_AST_PLACEHOLDER_CTE = 25,
    MYLITE_SQL_AST_PLACEHOLDER_CHECK_TABLE = 26,
    MYLITE_SQL_AST_PLACEHOLDER_OPTIMIZE_TABLE = 27,
    MYLITE_SQL_AST_PLACEHOLDER_REPAIR_TABLE = 28,
    MYLITE_SQL_AST_PLACEHOLDER_LOCK_TABLES = 29,
    MYLITE_SQL_AST_PLACEHOLDER_UNLOCK_TABLES = 30,
};

enum mylite_sql_ast_delete_form {
    MYLITE_SQL_AST_DELETE_SINGLE_TABLE = 0,
    MYLITE_SQL_AST_DELETE_TARGETS_FROM = 1,
    MYLITE_SQL_AST_DELETE_FROM_TARGETS_USING = 2,
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

enum mylite_sql_ast_show_diagnostics_kind {
    MYLITE_SQL_AST_SHOW_DIAGNOSTICS_WARNINGS = 0,
    MYLITE_SQL_AST_SHOW_DIAGNOSTICS_ERRORS = 1,
};

enum mylite_sql_ast_show_variables_scope {
    MYLITE_SQL_AST_SHOW_VARIABLES_SESSION = 0,
    MYLITE_SQL_AST_SHOW_VARIABLES_GLOBAL = 1,
};

enum mylite_sql_ast_show_status_scope {
    MYLITE_SQL_AST_SHOW_STATUS_SESSION = 0,
    MYLITE_SQL_AST_SHOW_STATUS_GLOBAL = 1,
};

enum mylite_sql_ast_set_system_variable_scope {
    MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_SESSION = 0,
    MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_GLOBAL = 1,
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
    MYLITE_SQL_AST_OPERATOR_REGEXP = 40,
    MYLITE_SQL_AST_OPERATOR_NOT_REGEXP = 41,
    MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT = 42,
    MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT = 43,
    MYLITE_SQL_AST_OPERATOR_BINARY_CAST = 44,
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
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_GENERATED = 13,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_REFERENCES = 14,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_CHECK = 15,
};

enum mylite_sql_ast_generated_column_storage {
    MYLITE_SQL_AST_GENERATED_COLUMN_STORAGE_NONE = 0,
    MYLITE_SQL_AST_GENERATED_COLUMN_STORAGE_VIRTUAL = 1,
    MYLITE_SQL_AST_GENERATED_COLUMN_STORAGE_STORED = 2,
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
    MYLITE_SQL_AST_INDEX_OPTION_WITH_PARSER = 8,
};

enum mylite_sql_ast_index_class {
    MYLITE_SQL_AST_INDEX_CLASS_ORDINARY = 0,
    MYLITE_SQL_AST_INDEX_CLASS_UNIQUE = 1,
    MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT = 2,
    MYLITE_SQL_AST_INDEX_CLASS_SPATIAL = 3,
};

enum mylite_sql_ast_ddl_table_option {
    MYLITE_SQL_AST_DDL_TABLE_OPTION_NONE = 0,
    MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM = 1,
    MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK = 2,
    MYLITE_SQL_AST_DDL_TABLE_OPTION_AUTO_INCREMENT = 3,
};

enum mylite_sql_ast_table_option {
    MYLITE_SQL_AST_TABLE_OPTION_NONE = 0,
    MYLITE_SQL_AST_TABLE_OPTION_ENGINE = 1,
    MYLITE_SQL_AST_TABLE_OPTION_CHARACTER_SET = 2,
    MYLITE_SQL_AST_TABLE_OPTION_COLLATE = 3,
    MYLITE_SQL_AST_TABLE_OPTION_COMMENT = 4,
    MYLITE_SQL_AST_TABLE_OPTION_AUTO_INCREMENT = 5,
};

enum mylite_sql_ast_alter_table_action {
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_NONE = 0,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_COLUMN = 1,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_COLUMN = 2,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_COLUMN = 3,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_CHANGE_COLUMN = 4,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_MODIFY_COLUMN = 5,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY = 6,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY = 7,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX = 8,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX = 9,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX = 10,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX = 11,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_INDEX = 12,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_INDEX = 13,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY = 14,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_CHECK = 15,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_CHECK_OR_CONSTRAINT = 16,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_CHECK_OR_CONSTRAINT = 17,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY = 18,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY = 19,
    MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_TABLE = 20,
};

enum mylite_sql_ast_alter_table_column_position {
    MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_NONE = 0,
    MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_FIRST = 1,
    MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_AFTER = 2,
};

enum mylite_sql_ast_alter_table_index_spelling {
    MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_NONE = 0,
    MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_INDEX = 1,
    MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_KEY = 2,
};

enum mylite_sql_ast_alter_table_constraint_spelling {
    MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_NONE = 0,
    MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_CHECK = 1,
    MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_CONSTRAINT = 2,
};

enum mylite_sql_ast_constraint_enforcement {
    MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_DEFAULT = 0,
    MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_ENFORCED = 1,
    MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_NOT_ENFORCED = 2,
};

enum mylite_sql_ast_reference_option {
    MYLITE_SQL_AST_REFERENCE_OPTION_NONE = 0,
    MYLITE_SQL_AST_REFERENCE_OPTION_ON_DELETE = 1,
    MYLITE_SQL_AST_REFERENCE_OPTION_ON_UPDATE = 2,
    MYLITE_SQL_AST_REFERENCE_OPTION_MATCH = 3,
};

enum mylite_sql_ast_reference_action {
    MYLITE_SQL_AST_REFERENCE_ACTION_NONE = 0,
    MYLITE_SQL_AST_REFERENCE_ACTION_RESTRICT = 1,
    MYLITE_SQL_AST_REFERENCE_ACTION_CASCADE = 2,
    MYLITE_SQL_AST_REFERENCE_ACTION_SET_NULL = 3,
    MYLITE_SQL_AST_REFERENCE_ACTION_NO_ACTION = 4,
    MYLITE_SQL_AST_REFERENCE_ACTION_SET_DEFAULT = 5,
};

enum mylite_sql_ast_reference_match {
    MYLITE_SQL_AST_REFERENCE_MATCH_NONE = 0,
    MYLITE_SQL_AST_REFERENCE_MATCH_SIMPLE = 1,
    MYLITE_SQL_AST_REFERENCE_MATCH_FULL = 2,
    MYLITE_SQL_AST_REFERENCE_MATCH_PARTIAL = 3,
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

enum mylite_sql_ast_aggregate_kind {
    MYLITE_SQL_AST_AGGREGATE_NONE = 0,
    MYLITE_SQL_AST_AGGREGATE_COUNT = 1,
    MYLITE_SQL_AST_AGGREGATE_SUM = 2,
    MYLITE_SQL_AST_AGGREGATE_AVG = 3,
    MYLITE_SQL_AST_AGGREGATE_MIN = 4,
    MYLITE_SQL_AST_AGGREGATE_MAX = 5,
    MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT = 6,
};

enum mylite_sql_ast_aggregate_argument {
    MYLITE_SQL_AST_AGGREGATE_ARGUMENT_NONE = 0,
    MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR = 1,
    MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION = 2,
    MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST = 3,
    MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION_LIST = 4,
};

enum mylite_sql_ast_join_type {
    MYLITE_SQL_AST_JOIN_NONE = 0,
    MYLITE_SQL_AST_JOIN_INNER = 1,
    MYLITE_SQL_AST_JOIN_CROSS = 2,
    MYLITE_SQL_AST_JOIN_COMMA = 3,
    MYLITE_SQL_AST_JOIN_LEFT = 4,
    MYLITE_SQL_AST_JOIN_RIGHT = 5,
};

enum mylite_sql_ast_join_condition_type {
    MYLITE_SQL_AST_JOIN_CONDITION_NONE = 0,
    MYLITE_SQL_AST_JOIN_CONDITION_ON = 1,
    MYLITE_SQL_AST_JOIN_CONDITION_USING = 2,
};

enum mylite_sql_ast_select_duplicate_mode {
    MYLITE_SQL_AST_SELECT_DUPLICATES_IMPLICIT_ALL = 0,
    MYLITE_SQL_AST_SELECT_DUPLICATES_ALL = 1,
    MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT = 2,
};

enum mylite_sql_ast_set_duplicate_mode {
    MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT = 0,
    MYLITE_SQL_AST_SET_DUPLICATES_ALL = 1,
};

enum mylite_sql_ast_set_operation {
    MYLITE_SQL_AST_SET_OPERATION_UNION = 0,
    MYLITE_SQL_AST_SET_OPERATION_INTERSECT = 1,
    MYLITE_SQL_AST_SET_OPERATION_EXCEPT = 2,
};

enum mylite_sql_ast_window_function_kind {
    MYLITE_SQL_AST_WINDOW_FUNCTION_NONE = 0,
    MYLITE_SQL_AST_WINDOW_FUNCTION_AGGREGATE = 1,
    MYLITE_SQL_AST_WINDOW_FUNCTION_CUME_DIST = 2,
    MYLITE_SQL_AST_WINDOW_FUNCTION_DENSE_RANK = 3,
    MYLITE_SQL_AST_WINDOW_FUNCTION_FIRST_VALUE = 4,
    MYLITE_SQL_AST_WINDOW_FUNCTION_LAG = 5,
    MYLITE_SQL_AST_WINDOW_FUNCTION_LAST_VALUE = 6,
    MYLITE_SQL_AST_WINDOW_FUNCTION_LEAD = 7,
    MYLITE_SQL_AST_WINDOW_FUNCTION_NTH_VALUE = 8,
    MYLITE_SQL_AST_WINDOW_FUNCTION_NTILE = 9,
    MYLITE_SQL_AST_WINDOW_FUNCTION_PERCENT_RANK = 10,
    MYLITE_SQL_AST_WINDOW_FUNCTION_RANK = 11,
    MYLITE_SQL_AST_WINDOW_FUNCTION_ROW_NUMBER = 12,
};

enum mylite_sql_ast_window_frame_unit {
    MYLITE_SQL_AST_WINDOW_FRAME_UNIT_NONE = 0,
    MYLITE_SQL_AST_WINDOW_FRAME_UNIT_ROWS = 1,
    MYLITE_SQL_AST_WINDOW_FRAME_UNIT_RANGE = 2,
};

enum mylite_sql_ast_window_frame_bound_kind {
    MYLITE_SQL_AST_WINDOW_FRAME_BOUND_NONE = 0,
    MYLITE_SQL_AST_WINDOW_FRAME_BOUND_CURRENT_ROW = 1,
    MYLITE_SQL_AST_WINDOW_FRAME_BOUND_UNBOUNDED_PRECEDING = 2,
    MYLITE_SQL_AST_WINDOW_FRAME_BOUND_UNBOUNDED_FOLLOWING = 3,
    MYLITE_SQL_AST_WINDOW_FRAME_BOUND_EXPRESSION_PRECEDING = 4,
    MYLITE_SQL_AST_WINDOW_FRAME_BOUND_EXPRESSION_FOLLOWING = 5,
};

enum mylite_sql_ast_window_null_treatment {
    MYLITE_SQL_AST_WINDOW_NULL_TREATMENT_NONE = 0,
    MYLITE_SQL_AST_WINDOW_NULL_TREATMENT_RESPECT = 1,
    MYLITE_SQL_AST_WINDOW_NULL_TREATMENT_IGNORE = 2,
};

enum mylite_sql_ast_subquery_quantifier {
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_NONE = 0,
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY = 1,
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME = 2,
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL = 3,
};

enum mylite_sql_ast_trim_direction {
    MYLITE_SQL_AST_TRIM_DIRECTION_NONE = 0,
    MYLITE_SQL_AST_TRIM_DIRECTION_BOTH = 1,
    MYLITE_SQL_AST_TRIM_DIRECTION_LEADING = 2,
    MYLITE_SQL_AST_TRIM_DIRECTION_TRAILING = 3,
};

enum mylite_sql_ast_interval_unit {
    MYLITE_SQL_AST_INTERVAL_UNIT_NONE = 0,
    MYLITE_SQL_AST_INTERVAL_UNIT_DAY = 1,
    MYLITE_SQL_AST_INTERVAL_UNIT_WEEK = 2,
    MYLITE_SQL_AST_INTERVAL_UNIT_MONTH = 3,
    MYLITE_SQL_AST_INTERVAL_UNIT_YEAR = 4,
    MYLITE_SQL_AST_INTERVAL_UNIT_HOUR = 5,
    MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE = 6,
    MYLITE_SQL_AST_INTERVAL_UNIT_SECOND = 7,
};

struct mylite_sql_ast_select_duplicate_mode_spans {
    struct mylite_sql_source_span first;
    struct mylite_sql_source_span last;
    struct mylite_sql_source_span conflict;
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
    size_t select_duplicate_modifier_count;
    struct mylite_sql_source_span span;
    struct mylite_sql_source_span column_character_set;
    struct mylite_sql_source_span column_collation;
    struct mylite_sql_source_span select_duplicate_first_span;
    struct mylite_sql_source_span select_duplicate_last_span;
    struct mylite_sql_source_span select_duplicate_conflict_span;
    enum mylite_sql_ast_node_kind kind;
    enum mylite_sql_ast_literal_kind literal_kind;
    enum mylite_sql_ast_operator operator_kind;
    enum mylite_sql_ast_schema_option schema_option;
    enum mylite_sql_ast_column_type column_type;
    enum mylite_sql_ast_column_attribute column_attribute;
    enum mylite_sql_ast_column_format column_format;
    enum mylite_sql_ast_column_storage column_storage;
    enum mylite_sql_ast_generated_column_storage generated_column_storage;
    enum mylite_sql_ast_key_part_order key_part_order;
    enum mylite_sql_ast_index_algorithm index_algorithm;
    enum mylite_sql_ast_index_option index_option;
    enum mylite_sql_ast_index_class index_class;
    enum mylite_sql_ast_ddl_table_option ddl_table_option;
    enum mylite_sql_ast_table_option table_option;
    enum mylite_sql_ast_alter_table_action alter_table_action;
    enum mylite_sql_ast_alter_table_column_position alter_table_column_position;
    enum mylite_sql_ast_alter_table_index_spelling alter_table_index_spelling;
    enum mylite_sql_ast_alter_table_constraint_spelling alter_table_constraint_spelling;
    enum mylite_sql_ast_constraint_enforcement constraint_enforcement;
    enum mylite_sql_ast_reference_option reference_option;
    enum mylite_sql_ast_reference_action reference_action;
    enum mylite_sql_ast_reference_match reference_match;
    enum mylite_sql_ast_transaction_access_mode transaction_access_mode;
    enum mylite_sql_ast_transaction_chain transaction_chain;
    enum mylite_sql_ast_transaction_release transaction_release;
    enum mylite_sql_ast_aggregate_kind aggregate_kind;
    enum mylite_sql_ast_aggregate_argument aggregate_argument;
    enum mylite_sql_ast_join_type join_type;
    enum mylite_sql_ast_join_condition_type join_condition_type;
    enum mylite_sql_ast_select_duplicate_mode select_duplicate_mode;
    enum mylite_sql_ast_set_duplicate_mode set_duplicate_mode;
    enum mylite_sql_ast_set_operation set_operation;
    enum mylite_sql_ast_window_function_kind window_function_kind;
    enum mylite_sql_ast_window_frame_unit window_frame_unit;
    enum mylite_sql_ast_window_frame_bound_kind window_frame_bound_kind;
    enum mylite_sql_ast_window_null_treatment window_null_treatment;
    enum mylite_sql_ast_subquery_quantifier subquery_quantifier;
    enum mylite_sql_ast_trim_direction trim_direction;
    enum mylite_sql_ast_interval_unit interval_unit;
    enum mylite_sql_ast_placeholder_statement_kind placeholder_statement_kind;
    enum mylite_sql_ast_show_diagnostics_kind show_diagnostics_kind;
    enum mylite_sql_ast_show_variables_scope show_variables_scope;
    enum mylite_sql_ast_show_status_scope show_status_scope;
    enum mylite_sql_ast_set_system_variable_scope set_system_variable_scope;
    enum mylite_sql_ast_delete_form delete_form;
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
    bool create_table_temporary;
    bool drop_table_temporary;
    bool drop_table_restrict;
    bool drop_table_cascade;
    bool insert_ignore;
    bool replace_low_priority;
    bool replace_delayed;
    bool alter_table_action_column_keyword;
    bool select_duplicate_mode_explicit;
    bool select_duplicate_mode_conflict;
    bool select_calc_found_rows;
    bool begin_transaction_immediate;
    bool transaction_consistent_snapshot;
    bool show_tables_extended;
    bool show_tables_full;
    bool show_columns_extended;
    bool show_columns_full;
    bool show_index_extended;
    bool show_create_schema_if_not_exists;
    bool trim_spec;
    bool interval_spec;
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
void mylite_sql_ast_node_set_delete_form(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_delete_form delete_form
);
void mylite_sql_ast_node_set_operator(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator operator_kind
);
void mylite_sql_ast_node_set_schema_option(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_schema_option schema_option
);
void mylite_sql_ast_node_set_column_type(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_type column_type
);
void mylite_sql_ast_node_set_column_type_signed(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_type_unsigned(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_display_width(
    struct mylite_sql_ast_node *node,
    unsigned int display_width
);
void mylite_sql_ast_node_set_column_length(struct mylite_sql_ast_node *node, uint64_t length);
void mylite_sql_ast_node_set_column_precision(struct mylite_sql_ast_node *node, uint64_t precision);
void mylite_sql_ast_node_set_column_scale(struct mylite_sql_ast_node *node, uint64_t scale);
void mylite_sql_ast_node_set_column_character_set(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_source_span span
);
void mylite_sql_ast_node_set_column_collation(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_source_span span
);
void mylite_sql_ast_node_set_column_binary_attribute(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_byte_attribute(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_zerofill_attribute(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_national_attribute(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_column_attribute(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_attribute column_attribute
);
void mylite_sql_ast_node_set_column_format(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_format column_format
);
void mylite_sql_ast_node_set_column_storage(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_storage column_storage
);
void mylite_sql_ast_node_set_key_part_order(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_key_part_order order
);
void mylite_sql_ast_node_set_limit_bound_value(struct mylite_sql_ast_node *node, uint64_t value);
void mylite_sql_ast_node_set_index_algorithm(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_index_algorithm algorithm
);
void mylite_sql_ast_node_set_index_option(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_index_option option
);
void mylite_sql_ast_node_set_index_class(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_index_class index_class
);
void mylite_sql_ast_node_set_ddl_table_option(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_ddl_table_option option
);
void mylite_sql_ast_node_set_table_option(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_table_option option
);
void mylite_sql_ast_node_set_alter_table_action(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_alter_table_action action,
    bool column_keyword
);
void mylite_sql_ast_node_set_alter_table_column_position(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_alter_table_column_position position
);
void mylite_sql_ast_node_set_alter_table_index_spelling(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_alter_table_index_spelling spelling
);
void mylite_sql_ast_node_set_alter_table_constraint_spelling(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_alter_table_constraint_spelling spelling
);
void mylite_sql_ast_node_set_constraint_enforcement(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_constraint_enforcement enforcement
);
void mylite_sql_ast_node_set_reference_option(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_reference_option option
);
void mylite_sql_ast_node_set_reference_action(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_reference_action action
);
void mylite_sql_ast_node_set_reference_match(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_reference_match match
);
void mylite_sql_ast_node_set_create_table_temporary(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_drop_table_temporary(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_drop_table_restrict(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_drop_table_cascade(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_insert_ignore(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_replace_low_priority(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_replace_delayed(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_transaction_access_mode(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_transaction_access_mode access_mode
);
void mylite_sql_ast_node_set_begin_transaction_immediate(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_transaction_consistent_snapshot(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_transaction_completion(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_transaction_chain chain,
    enum mylite_sql_ast_transaction_release release
);
void mylite_sql_ast_node_set_case_expression_simple(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_aggregate(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_aggregate_kind aggregate_kind,
    enum mylite_sql_ast_aggregate_argument aggregate_argument
);
void mylite_sql_ast_node_set_join_type(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_join_type join_type
);
void mylite_sql_ast_node_set_join_condition_type(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_join_condition_type condition_type
);
void mylite_sql_ast_node_set_select_duplicate_mode(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_select_duplicate_mode mode,
    bool explicit_mode,
    bool conflict,
    size_t modifier_count,
    struct mylite_sql_ast_select_duplicate_mode_spans spans
);
void mylite_sql_ast_node_set_select_calc_found_rows(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_set_duplicate_mode(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_set_duplicate_mode mode
);
void mylite_sql_ast_node_set_set_operation(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_set_operation operation
);
void mylite_sql_ast_node_set_window_function(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_window_function_kind function_kind
);
void mylite_sql_ast_node_set_window_frame_unit(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_window_frame_unit unit
);
void mylite_sql_ast_node_set_window_frame_bound(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_window_frame_bound_kind bound_kind
);
void mylite_sql_ast_node_set_window_null_treatment(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_window_null_treatment treatment
);
void mylite_sql_ast_node_set_subquery_quantifier(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_subquery_quantifier quantifier
);
void mylite_sql_ast_node_set_trim_spec(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_trim_direction direction
);
void mylite_sql_ast_node_set_interval_spec(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_interval_unit unit
);
void mylite_sql_ast_node_set_placeholder_statement_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_placeholder_statement_kind kind
);
void mylite_sql_ast_node_set_show_tables_extended(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_show_tables_full(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_show_columns_extended(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_show_columns_full(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_show_index_extended(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_show_create_schema_if_not_exists(struct mylite_sql_ast_node *node);
void mylite_sql_ast_node_set_show_diagnostics_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_show_diagnostics_kind kind
);
void mylite_sql_ast_node_set_show_variables_scope(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_show_variables_scope scope
);
void mylite_sql_ast_node_set_show_status_scope(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_show_status_scope scope
);
void mylite_sql_ast_node_set_system_variable_scope(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_set_system_variable_scope scope
);

const struct mylite_sql_ast_node *mylite_sql_ast_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
);
size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node);

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind);
const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind);
const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind);
const char *mylite_sql_ast_schema_option_name(enum mylite_sql_ast_schema_option schema_option);
const char *mylite_sql_ast_column_type_name(enum mylite_sql_ast_column_type column_type);
const char *mylite_sql_ast_column_attribute_name(
    enum mylite_sql_ast_column_attribute column_attribute
);
const char *mylite_sql_ast_column_format_name(enum mylite_sql_ast_column_format column_format);
const char *mylite_sql_ast_column_storage_name(enum mylite_sql_ast_column_storage column_storage);
const char *mylite_sql_ast_key_part_order_name(enum mylite_sql_ast_key_part_order order);
const char *mylite_sql_ast_index_algorithm_name(enum mylite_sql_ast_index_algorithm algorithm);
const char *mylite_sql_ast_index_option_name(enum mylite_sql_ast_index_option option);
const char *mylite_sql_ast_index_class_name(enum mylite_sql_ast_index_class index_class);
const char *mylite_sql_ast_ddl_table_option_name(enum mylite_sql_ast_ddl_table_option option);
const char *mylite_sql_ast_alter_table_action_name(enum mylite_sql_ast_alter_table_action action);
const char *mylite_sql_ast_alter_table_column_position_name(
    enum mylite_sql_ast_alter_table_column_position position
);
const char *mylite_sql_ast_alter_table_index_spelling_name(
    enum mylite_sql_ast_alter_table_index_spelling spelling
);
const char *mylite_sql_ast_alter_table_constraint_spelling_name(
    enum mylite_sql_ast_alter_table_constraint_spelling spelling
);
const char *mylite_sql_ast_constraint_enforcement_name(
    enum mylite_sql_ast_constraint_enforcement enforcement
);
const char *mylite_sql_ast_reference_option_name(enum mylite_sql_ast_reference_option option);
const char *mylite_sql_ast_reference_action_name(enum mylite_sql_ast_reference_action action);
const char *mylite_sql_ast_reference_match_name(enum mylite_sql_ast_reference_match match);
const char *mylite_sql_ast_aggregate_kind_name(enum mylite_sql_ast_aggregate_kind aggregate_kind);
const char *mylite_sql_ast_aggregate_argument_name(
    enum mylite_sql_ast_aggregate_argument aggregate_argument
);
const char *mylite_sql_ast_join_type_name(enum mylite_sql_ast_join_type join_type);
const char *mylite_sql_ast_join_condition_type_name(
    enum mylite_sql_ast_join_condition_type condition_type
);
const char *mylite_sql_ast_select_duplicate_mode_name(
    enum mylite_sql_ast_select_duplicate_mode mode
);
const char *mylite_sql_ast_set_duplicate_mode_name(enum mylite_sql_ast_set_duplicate_mode mode);
const char *mylite_sql_ast_set_operation_name(enum mylite_sql_ast_set_operation operation);
const char *mylite_sql_ast_subquery_quantifier_name(
    enum mylite_sql_ast_subquery_quantifier quantifier
);

#endif
