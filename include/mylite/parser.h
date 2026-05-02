#ifndef MYLITE_PARSER_H
#define MYLITE_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MyliteParseStatus {
  MYLITE_PARSE_OK = 0,
  MYLITE_PARSE_SYNTAX_ERROR = 1,
  MYLITE_PARSE_LEX_ERROR = 2,
  MYLITE_PARSE_NO_MEMORY = 3
} MyliteParseStatus;

typedef struct MyliteParseResult {
  MyliteParseStatus status;
  size_t offset;
  int token;
  char message[160];
} MyliteParseResult;

typedef struct MyliteAst MyliteAst;
typedef struct MyliteAstNode MyliteAstNode;

typedef enum MyliteAstNodeKind {
  MYLITE_AST_NODE_RULE = 1,
  MYLITE_AST_NODE_TOKEN = 2
} MyliteAstNodeKind;

typedef enum MyliteStatementKind {
  MYLITE_STATEMENT_UNKNOWN = 0,
  MYLITE_STATEMENT_EMPTY,
  MYLITE_STATEMENT_SELECT,
  MYLITE_STATEMENT_INSERT,
  MYLITE_STATEMENT_UPDATE,
  MYLITE_STATEMENT_DELETE,
  MYLITE_STATEMENT_REPLACE,
  MYLITE_STATEMENT_CREATE,
  MYLITE_STATEMENT_ALTER,
  MYLITE_STATEMENT_DROP,
  MYLITE_STATEMENT_RENAME,
  MYLITE_STATEMENT_TRUNCATE,
  MYLITE_STATEMENT_SET,
  MYLITE_STATEMENT_SHOW,
  MYLITE_STATEMENT_EXPLAIN,
  MYLITE_STATEMENT_DO,
  MYLITE_STATEMENT_CALL,
  MYLITE_STATEMENT_PREPARE,
  MYLITE_STATEMENT_EXECUTE,
  MYLITE_STATEMENT_DEALLOCATE,
  MYLITE_STATEMENT_TRANSACTION,
  MYLITE_STATEMENT_LOCK,
  MYLITE_STATEMENT_UTILITY
} MyliteStatementKind;

typedef enum MyliteStatementTargetKind {
  MYLITE_STATEMENT_TARGET_NONE = 0,
  MYLITE_STATEMENT_TARGET_TABLE,
  MYLITE_STATEMENT_TARGET_DATABASE,
  MYLITE_STATEMENT_TARGET_VIEW,
  MYLITE_STATEMENT_TARGET_ROUTINE,
  MYLITE_STATEMENT_TARGET_ACCOUNT,
  MYLITE_STATEMENT_TARGET_VARIABLE,
  MYLITE_STATEMENT_TARGET_UNKNOWN
} MyliteStatementTargetKind;

typedef enum MyliteStatementTargetRole {
  MYLITE_STATEMENT_TARGET_ROLE_NONE = 0,
  MYLITE_STATEMENT_TARGET_ROLE_PRIMARY,
  MYLITE_STATEMENT_TARGET_ROLE_SOURCE,
  MYLITE_STATEMENT_TARGET_ROLE_DESTINATION
} MyliteStatementTargetRole;

typedef enum MyliteCreateTableColumnTypeFamily {
  MYLITE_CREATE_TABLE_COLUMN_TYPE_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_JSON,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_ENUM,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_SET,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_SPATIAL
} MyliteCreateTableColumnTypeFamily;

typedef enum MyliteCreateTableColumnTypeKind {
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYINT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SMALLINT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMINT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIGINT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BOOL,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DECIMAL,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_FLOAT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_REAL,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DOUBLE,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_CHAR,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARCHAR,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NCHAR,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BINARY,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARBINARY,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYBLOB,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BLOB,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMBLOB,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGBLOB,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYTEXT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TEXT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMTEXT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGTEXT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_ENUM,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SET,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_JSON,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARCHAR,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARBINARY,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VECTOR,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATE,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATETIME,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIMESTAMP,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIME,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_YEAR,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRY,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POINT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LINESTRING,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POLYGON,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOINT,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTILINESTRING,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOLYGON,
  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRYCOLLECTION
} MyliteCreateTableColumnTypeKind;

typedef enum MyliteCreateTableColumnStorageClass {
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_DECIMAL,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_FLOAT,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_BIT,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_FIXED_STRING,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_VARIABLE_STRING,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_BINARY_STRING,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_BLOB,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEXT,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_ENUM,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_SET,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_JSON,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEMPORAL,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_SPATIAL,
  MYLITE_CREATE_TABLE_COLUMN_STORAGE_VECTOR
} MyliteCreateTableColumnStorageClass;

typedef enum MyliteCreateTableColumnFlag {
  MYLITE_CREATE_TABLE_COLUMN_FLAG_NONE = 0,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_NOT_NULL = 1u << 0,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_NULL = 1u << 1,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_DEFAULT = 1u << 2,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_AUTO_INCREMENT = 1u << 3,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_PRIMARY_KEY = 1u << 4,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_UNIQUE_KEY = 1u << 5,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_COMMENT = 1u << 6,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_GENERATED = 1u << 7,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_VIRTUAL = 1u << 8,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_STORED = 1u << 9,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_ON_UPDATE = 1u << 10,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_REFERENCES = 1u << 11,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_CHECK = 1u << 12,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_UNSIGNED = 1u << 13,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_ZEROFILL = 1u << 14,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_CHARACTER_SET = 1u << 15,
  MYLITE_CREATE_TABLE_COLUMN_FLAG_COLLATE = 1u << 16
} MyliteCreateTableColumnFlag;

typedef enum MyliteCreateTableKeyKind {
  MYLITE_CREATE_TABLE_KEY_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_KEY_PRIMARY,
  MYLITE_CREATE_TABLE_KEY_INDEX,
  MYLITE_CREATE_TABLE_KEY_UNIQUE,
  MYLITE_CREATE_TABLE_KEY_FULLTEXT,
  MYLITE_CREATE_TABLE_KEY_SPATIAL,
  MYLITE_CREATE_TABLE_KEY_FOREIGN,
  MYLITE_CREATE_TABLE_KEY_CHECK
} MyliteCreateTableKeyKind;

typedef enum MyliteCreateTableKeyPartKind {
  MYLITE_CREATE_TABLE_KEY_PART_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_KEY_PART_COLUMN,
  MYLITE_CREATE_TABLE_KEY_PART_EXPRESSION
} MyliteCreateTableKeyPartKind;

typedef enum MyliteCreateTableKeyPartOrder {
  MYLITE_CREATE_TABLE_KEY_PART_ORDER_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_KEY_PART_ORDER_ASC,
  MYLITE_CREATE_TABLE_KEY_PART_ORDER_DESC
} MyliteCreateTableKeyPartOrder;

typedef enum MyliteCreateTableKeyOptionKind {
  MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE,
  MYLITE_CREATE_TABLE_KEY_OPTION_KEY_BLOCK_SIZE,
  MYLITE_CREATE_TABLE_KEY_OPTION_COMMENT,
  MYLITE_CREATE_TABLE_KEY_OPTION_WITH_PARSER,
  MYLITE_CREATE_TABLE_KEY_OPTION_VISIBLE,
  MYLITE_CREATE_TABLE_KEY_OPTION_INVISIBLE,
  MYLITE_CREATE_TABLE_KEY_OPTION_SECONDARY_ENGINE_ATTRIBUTE,
  MYLITE_CREATE_TABLE_KEY_OPTION_WHERE
} MyliteCreateTableKeyOptionKind;

typedef enum MyliteCreateTableForeignMatchKind {
  MYLITE_CREATE_TABLE_FOREIGN_MATCH_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_FOREIGN_MATCH_FULL,
  MYLITE_CREATE_TABLE_FOREIGN_MATCH_PARTIAL,
  MYLITE_CREATE_TABLE_FOREIGN_MATCH_SIMPLE
} MyliteCreateTableForeignMatchKind;

typedef enum MyliteCreateTableForeignAction {
  MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_FOREIGN_ACTION_RESTRICT,
  MYLITE_CREATE_TABLE_FOREIGN_ACTION_CASCADE,
  MYLITE_CREATE_TABLE_FOREIGN_ACTION_SET_NULL,
  MYLITE_CREATE_TABLE_FOREIGN_ACTION_NO_ACTION,
  MYLITE_CREATE_TABLE_FOREIGN_ACTION_SET_DEFAULT
} MyliteCreateTableForeignAction;

typedef enum MyliteCreateTableCheckEnforcement {
  MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_ENFORCED,
  MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_NOT_ENFORCED
} MyliteCreateTableCheckEnforcement;

typedef enum MyliteCreateTableOptionKind {
  MYLITE_CREATE_TABLE_OPTION_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_OPTION_ENGINE,
  MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE,
  MYLITE_CREATE_TABLE_OPTION_CHARSET,
  MYLITE_CREATE_TABLE_OPTION_COLLATE,
  MYLITE_CREATE_TABLE_OPTION_AUTO_INCREMENT,
  MYLITE_CREATE_TABLE_OPTION_COMMENT,
  MYLITE_CREATE_TABLE_OPTION_ROW_FORMAT,
  MYLITE_CREATE_TABLE_OPTION_KEY_BLOCK_SIZE,
  MYLITE_CREATE_TABLE_OPTION_AUTOEXTEND_SIZE,
  MYLITE_CREATE_TABLE_OPTION_AVG_ROW_LENGTH,
  MYLITE_CREATE_TABLE_OPTION_MAX_ROWS,
  MYLITE_CREATE_TABLE_OPTION_MIN_ROWS,
  MYLITE_CREATE_TABLE_OPTION_DELAY_KEY_WRITE,
  MYLITE_CREATE_TABLE_OPTION_ENCRYPTION,
  MYLITE_CREATE_TABLE_OPTION_STATS_PERSISTENT,
  MYLITE_CREATE_TABLE_OPTION_PACK_KEYS,
  MYLITE_CREATE_TABLE_OPTION_TABLESPACE,
  MYLITE_CREATE_TABLE_OPTION_STORAGE,
  MYLITE_CREATE_TABLE_OPTION_COMPRESSION,
  MYLITE_CREATE_TABLE_OPTION_CONNECTION,
  MYLITE_CREATE_TABLE_OPTION_PASSWORD,
  MYLITE_CREATE_TABLE_OPTION_INSERT_METHOD,
  MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY,
  MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY,
  MYLITE_CREATE_TABLE_OPTION_UNION,
  MYLITE_CREATE_TABLE_OPTION_ENGINE_ATTRIBUTE,
  MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE_ATTRIBUTE
} MyliteCreateTableOptionKind;

MyliteParseStatus mylite_parse_sql(const char *sql, MyliteParseResult *result);
MyliteParseStatus mylite_parse_sql_ast(const char *sql, MyliteAst **ast,
                                       MyliteParseResult *result);
const char *mylite_parse_status_name(MyliteParseStatus status);
const char *mylite_statement_kind_name(MyliteStatementKind kind);
const char *mylite_statement_target_kind_name(MyliteStatementTargetKind kind);
const char *mylite_statement_target_role_name(MyliteStatementTargetRole role);
const char *mylite_create_table_column_type_family_name(
    MyliteCreateTableColumnTypeFamily family);
const char *mylite_create_table_column_type_kind_name(
    MyliteCreateTableColumnTypeKind kind);
const char *mylite_create_table_column_storage_class_name(
    MyliteCreateTableColumnStorageClass storage_class);
const char *mylite_create_table_key_kind_name(MyliteCreateTableKeyKind kind);
const char *mylite_create_table_key_part_kind_name(
    MyliteCreateTableKeyPartKind kind);
const char *mylite_create_table_key_part_order_name(
    MyliteCreateTableKeyPartOrder order);
const char *mylite_create_table_key_option_kind_name(
    MyliteCreateTableKeyOptionKind kind);
const char *mylite_create_table_foreign_match_kind_name(
    MyliteCreateTableForeignMatchKind kind);
const char *mylite_create_table_foreign_action_name(
    MyliteCreateTableForeignAction action);
const char *mylite_create_table_check_enforcement_name(
    MyliteCreateTableCheckEnforcement enforcement);
const char *mylite_create_table_option_kind_name(
    MyliteCreateTableOptionKind kind);

void mylite_ast_free(MyliteAst *ast);
const MyliteAstNode *mylite_ast_root(const MyliteAst *ast);
size_t mylite_ast_node_count(const MyliteAst *ast);
size_t mylite_ast_allocated_bytes(const MyliteAst *ast);
size_t mylite_ast_statement_count(const MyliteAst *ast);
MyliteStatementKind mylite_ast_statement_kind(const MyliteAst *ast, size_t index);
const char *mylite_ast_statement_symbol_name(const MyliteAst *ast, size_t index);
const MyliteAstNode *mylite_ast_statement_node(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_end(const MyliteAst *ast, size_t index);
MyliteStatementTargetKind mylite_ast_statement_target_kind(const MyliteAst *ast,
                                                           size_t index);
size_t mylite_ast_statement_target_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_end(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_schema_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_schema_end(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_name_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_name_end(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_count(const MyliteAst *ast, size_t statement_index);
MyliteStatementTargetKind mylite_ast_statement_target_kind_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index);
MyliteStatementTargetRole mylite_ast_statement_target_role_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index);
size_t mylite_ast_statement_target_start_at(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t target_index);
size_t mylite_ast_statement_target_end_at(const MyliteAst *ast,
                                          size_t statement_index,
                                          size_t target_index);
size_t mylite_ast_statement_target_schema_start_at(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t target_index);
size_t mylite_ast_statement_target_schema_end_at(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t target_index);
size_t mylite_ast_statement_target_name_start_at(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t target_index);
size_t mylite_ast_statement_target_name_end_at(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t target_index);
size_t mylite_ast_create_table_column_count(const MyliteAst *ast,
                                            size_t statement_index);
size_t mylite_ast_create_table_column_start(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t column_index);
size_t mylite_ast_create_table_column_end(const MyliteAst *ast,
                                          size_t statement_index,
                                          size_t column_index);
size_t mylite_ast_create_table_column_name_start(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t column_index);
size_t mylite_ast_create_table_column_name_end(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t column_index);
size_t mylite_ast_create_table_column_type_start(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t column_index);
size_t mylite_ast_create_table_column_type_end(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t column_index);
size_t mylite_ast_create_table_column_type_name_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index);
size_t mylite_ast_create_table_column_type_name_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index);
size_t mylite_ast_create_table_column_type_parameters_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_parameters_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_numeric_parameter_count(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
unsigned long long mylite_ast_create_table_column_type_numeric_parameter_at(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t parameter_index);
size_t mylite_ast_create_table_column_type_element_count(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
int mylite_ast_create_table_column_type_has_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
unsigned long long mylite_ast_create_table_column_type_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
int mylite_ast_create_table_column_type_has_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
unsigned long long mylite_ast_create_table_column_type_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
int mylite_ast_create_table_column_type_has_scale(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
unsigned long long mylite_ast_create_table_column_type_scale(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
int mylite_ast_create_table_column_type_has_fractional_seconds_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
unsigned long long
mylite_ast_create_table_column_type_fractional_seconds_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_attributes_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_attributes_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_options_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index);
size_t mylite_ast_create_table_column_options_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index);
size_t mylite_ast_create_table_column_default_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index);
size_t mylite_ast_create_table_column_default_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index);
size_t mylite_ast_create_table_column_default_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_default_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_on_update_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index);
size_t mylite_ast_create_table_column_on_update_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index);
size_t mylite_ast_create_table_column_on_update_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_on_update_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_generated_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index);
size_t mylite_ast_create_table_column_generated_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index);
size_t mylite_ast_create_table_column_generated_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_generated_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_generated_storage_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_generated_storage_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_comment_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index);
size_t mylite_ast_create_table_column_comment_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index);
size_t mylite_ast_create_table_column_comment_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_comment_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_check_start(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index);
size_t mylite_ast_create_table_column_check_end(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t column_index);
size_t mylite_ast_create_table_column_check_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_check_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
MyliteCreateTableCheckEnforcement
mylite_ast_create_table_column_check_enforcement(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t column_index);
size_t mylite_ast_create_table_column_check_enforcement_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_check_enforcement_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_reference_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index);
size_t mylite_ast_create_table_column_reference_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_type_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_options_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_default_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_default_value_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_on_update_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_on_update_value_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_generated_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_generated_expression_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_generated_storage_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_comment_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_check_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_check_expression_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_check_enforcement_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
const MyliteAstNode *mylite_ast_create_table_column_reference_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
MyliteCreateTableColumnTypeFamily mylite_ast_create_table_column_type_family(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
MyliteCreateTableColumnTypeKind mylite_ast_create_table_column_type_kind(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
MyliteCreateTableColumnStorageClass
mylite_ast_create_table_column_storage_class(const MyliteAst *ast,
                                             size_t statement_index,
                                             size_t column_index);
unsigned int mylite_ast_create_table_column_flags(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index);
size_t mylite_ast_create_table_key_count(const MyliteAst *ast,
                                         size_t statement_index);
MyliteCreateTableKeyKind mylite_ast_create_table_key_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_start(const MyliteAst *ast,
                                         size_t statement_index,
                                         size_t key_index);
size_t mylite_ast_create_table_key_end(const MyliteAst *ast,
                                       size_t statement_index,
                                       size_t key_index);
size_t mylite_ast_create_table_key_constraint_name_start(const MyliteAst *ast,
                                                         size_t statement_index,
                                                         size_t key_index);
size_t mylite_ast_create_table_key_constraint_name_end(const MyliteAst *ast,
                                                       size_t statement_index,
                                                       size_t key_index);
size_t mylite_ast_create_table_key_name_start(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index);
size_t mylite_ast_create_table_key_name_end(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t key_index);
size_t mylite_ast_create_table_key_index_type_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t key_index);
size_t mylite_ast_create_table_key_index_type_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t key_index);
size_t mylite_ast_create_table_key_column_count(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index);
MyliteCreateTableKeyPartKind mylite_ast_create_table_key_column_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_start(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index,
                                                size_t column_index);
size_t mylite_ast_create_table_key_column_end(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index,
                                              size_t column_index);
size_t mylite_ast_create_table_key_column_name_start(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index,
                                                     size_t column_index);
size_t mylite_ast_create_table_key_column_name_end(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t key_index,
                                                   size_t column_index);
size_t mylite_ast_create_table_key_column_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_prefix_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_prefix_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_prefix_value_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_prefix_value_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
MyliteCreateTableKeyPartOrder mylite_ast_create_table_key_column_order(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_order_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_order_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_table_start(const MyliteAst *ast,
                                                          size_t statement_index,
                                                          size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_end(const MyliteAst *ast,
                                                        size_t statement_index,
                                                        size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_schema_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_schema_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_name_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_name_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_column_count(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
MyliteCreateTableKeyPartKind mylite_ast_create_table_key_referenced_column_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_column_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_column_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_column_name_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_column_name_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_column_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_column_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
MyliteCreateTableKeyPartOrder
mylite_ast_create_table_key_referenced_column_order(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
MyliteCreateTableForeignMatchKind
mylite_ast_create_table_key_foreign_match_kind(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t key_index);
size_t mylite_ast_create_table_key_foreign_match_start(const MyliteAst *ast,
                                                       size_t statement_index,
                                                       size_t key_index);
size_t mylite_ast_create_table_key_foreign_match_end(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index);
MyliteCreateTableForeignAction
mylite_ast_create_table_key_foreign_on_delete_action(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index);
size_t mylite_ast_create_table_key_foreign_on_delete_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_foreign_on_delete_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
MyliteCreateTableForeignAction
mylite_ast_create_table_key_foreign_on_update_action(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index);
size_t mylite_ast_create_table_key_foreign_on_update_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_foreign_on_update_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_check_expression_start(const MyliteAst *ast,
                                                          size_t statement_index,
                                                          size_t key_index);
size_t mylite_ast_create_table_key_check_expression_end(const MyliteAst *ast,
                                                        size_t statement_index,
                                                        size_t key_index);
MyliteCreateTableCheckEnforcement
mylite_ast_create_table_key_check_enforcement(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index);
size_t mylite_ast_create_table_key_check_enforcement_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_check_enforcement_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_option_count(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index);
MyliteCreateTableKeyOptionKind mylite_ast_create_table_key_option_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t option_index);
size_t mylite_ast_create_table_key_option_start(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index,
                                                size_t option_index);
size_t mylite_ast_create_table_key_option_end(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index,
                                              size_t option_index);
size_t mylite_ast_create_table_key_option_name_start(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index,
                                                     size_t option_index);
size_t mylite_ast_create_table_key_option_name_end(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t key_index,
                                                   size_t option_index);
size_t mylite_ast_create_table_key_option_value_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t key_index,
                                                      size_t option_index);
size_t mylite_ast_create_table_key_option_value_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t key_index,
                                                    size_t option_index);
size_t mylite_ast_create_table_option_count(const MyliteAst *ast,
                                            size_t statement_index);
MyliteCreateTableOptionKind mylite_ast_create_table_option_kind(
    const MyliteAst *ast, size_t statement_index, size_t option_index);
size_t mylite_ast_create_table_option_start(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t option_index);
size_t mylite_ast_create_table_option_end(const MyliteAst *ast,
                                          size_t statement_index,
                                          size_t option_index);
size_t mylite_ast_create_table_option_name_start(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t option_index);
size_t mylite_ast_create_table_option_name_end(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t option_index);
size_t mylite_ast_create_table_option_value_start(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t option_index);
size_t mylite_ast_create_table_option_value_end(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t option_index);
MyliteAstNodeKind mylite_ast_node_kind(const MyliteAstNode *node);
unsigned mylite_ast_node_rule_id(const MyliteAstNode *node);
const char *mylite_ast_node_symbol_name(const MyliteAstNode *node);
int mylite_ast_node_token(const MyliteAstNode *node);
size_t mylite_ast_node_start(const MyliteAstNode *node);
size_t mylite_ast_node_end(const MyliteAstNode *node);
size_t mylite_ast_node_child_count(const MyliteAstNode *node);
const MyliteAstNode *mylite_ast_node_child(const MyliteAstNode *node,
                                           size_t index);

#ifdef __cplusplus
}
#endif

#endif
