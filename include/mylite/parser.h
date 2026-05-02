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
typedef struct MyliteAstAlterTable MyliteAstAlterTable;
typedef struct MyliteAstAlterTableSpec MyliteAstAlterTableSpec;
typedef struct MyliteAstCreateIndex MyliteAstCreateIndex;
typedef struct MyliteAstCreateTable MyliteAstCreateTable;
typedef struct MyliteAstCreateTableColumn MyliteAstCreateTableColumn;
typedef struct MyliteAstCreateTableColumnTypeElement
    MyliteAstCreateTableColumnTypeElement;
typedef struct MyliteAstCreateTableKey MyliteAstCreateTableKey;
typedef struct MyliteAstCreateTableKeyPart MyliteAstCreateTableKeyPart;
typedef struct MyliteAstCreateTableKeyOption MyliteAstCreateTableKeyOption;
typedef struct MyliteAstCreateTableOption MyliteAstCreateTableOption;
typedef struct MyliteAstDropIndex MyliteAstDropIndex;
typedef struct MyliteAstDropTable MyliteAstDropTable;
typedef struct MyliteAstNode MyliteAstNode;
typedef struct MyliteAstRenameTable MyliteAstRenameTable;
typedef struct MyliteAstTruncateTable MyliteAstTruncateTable;

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

typedef enum MyliteCreateTableColumnNullability {
  MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_NULL,
  MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_NOT_NULL
} MyliteCreateTableColumnNullability;

typedef enum MyliteCreateTableColumnGeneratedStorage {
  MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_VIRTUAL,
  MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_STORED
} MyliteCreateTableColumnGeneratedStorage;

typedef enum MyliteCreateTableColumnValueKind {
  MYLITE_CREATE_TABLE_COLUMN_VALUE_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_COLUMN_VALUE_RAW,
  MYLITE_CREATE_TABLE_COLUMN_VALUE_STRING,
  MYLITE_CREATE_TABLE_COLUMN_VALUE_UNSIGNED_INTEGER,
  MYLITE_CREATE_TABLE_COLUMN_VALUE_NULL,
  MYLITE_CREATE_TABLE_COLUMN_VALUE_CURRENT_TIMESTAMP
} MyliteCreateTableColumnValueKind;

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

typedef enum MyliteCreateTableIndexType {
  MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_INDEX_TYPE_BTREE,
  MYLITE_CREATE_TABLE_INDEX_TYPE_HASH,
  MYLITE_CREATE_TABLE_INDEX_TYPE_RTREE
} MyliteCreateTableIndexType;

typedef enum MyliteCreateTableKeyVisibility {
  MYLITE_CREATE_TABLE_KEY_VISIBILITY_UNSPECIFIED = 0,
  MYLITE_CREATE_TABLE_KEY_VISIBILITY_VISIBLE,
  MYLITE_CREATE_TABLE_KEY_VISIBILITY_INVISIBLE
} MyliteCreateTableKeyVisibility;

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

typedef enum MyliteCreateTableKeyOptionValueKind {
  MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_RAW,
  MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_IDENTIFIER,
  MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_STRING,
  MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNSIGNED_INTEGER,
  MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_INDEX_TYPE
} MyliteCreateTableKeyOptionValueKind;

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

typedef enum MyliteCreateTableOptionValueKind {
  MYLITE_CREATE_TABLE_OPTION_VALUE_UNKNOWN = 0,
  MYLITE_CREATE_TABLE_OPTION_VALUE_RAW,
  MYLITE_CREATE_TABLE_OPTION_VALUE_IDENTIFIER,
  MYLITE_CREATE_TABLE_OPTION_VALUE_STRING,
  MYLITE_CREATE_TABLE_OPTION_VALUE_UNSIGNED_INTEGER,
  MYLITE_CREATE_TABLE_OPTION_VALUE_LIST
} MyliteCreateTableOptionValueKind;

typedef enum MyliteAlterTableSpecKind {
  MYLITE_ALTER_TABLE_SPEC_UNKNOWN = 0,
  MYLITE_ALTER_TABLE_SPEC_TABLE_OPTIONS,
  MYLITE_ALTER_TABLE_SPEC_CONVERT_CHARACTER_SET,
  MYLITE_ALTER_TABLE_SPEC_ADD_COLUMN,
  MYLITE_ALTER_TABLE_SPEC_ADD_TABLE_ELEMENTS,
  MYLITE_ALTER_TABLE_SPEC_ADD_CONSTRAINT,
  MYLITE_ALTER_TABLE_SPEC_ADD_PARTITION,
  MYLITE_ALTER_TABLE_SPEC_DROP_COLUMN,
  MYLITE_ALTER_TABLE_SPEC_DROP_PRIMARY_KEY,
  MYLITE_ALTER_TABLE_SPEC_DROP_INDEX,
  MYLITE_ALTER_TABLE_SPEC_DROP_FOREIGN_KEY,
  MYLITE_ALTER_TABLE_SPEC_DROP_CHECK,
  MYLITE_ALTER_TABLE_SPEC_DROP_PARTITION,
  MYLITE_ALTER_TABLE_SPEC_MODIFY_COLUMN,
  MYLITE_ALTER_TABLE_SPEC_CHANGE_COLUMN,
  MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_SET_DEFAULT,
  MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_DROP_DEFAULT,
  MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_VISIBILITY,
  MYLITE_ALTER_TABLE_SPEC_RENAME_COLUMN,
  MYLITE_ALTER_TABLE_SPEC_RENAME_TABLE,
  MYLITE_ALTER_TABLE_SPEC_RENAME_INDEX,
  MYLITE_ALTER_TABLE_SPEC_ORDER_BY,
  MYLITE_ALTER_TABLE_SPEC_DISABLE_KEYS,
  MYLITE_ALTER_TABLE_SPEC_ENABLE_KEYS,
  MYLITE_ALTER_TABLE_SPEC_LOCK,
  MYLITE_ALTER_TABLE_SPEC_ALGORITHM,
  MYLITE_ALTER_TABLE_SPEC_FORCE,
  MYLITE_ALTER_TABLE_SPEC_VALIDATION,
  MYLITE_ALTER_TABLE_SPEC_ALTER_CHECK,
  MYLITE_ALTER_TABLE_SPEC_ALTER_INDEX_VISIBILITY,
  MYLITE_ALTER_TABLE_SPEC_TABLESPACE,
  MYLITE_ALTER_TABLE_SPEC_PARTITION,
  MYLITE_ALTER_TABLE_SPEC_SECONDARY_LOAD,
  MYLITE_ALTER_TABLE_SPEC_SECONDARY_UNLOAD
} MyliteAlterTableSpecKind;

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
const char *mylite_create_table_column_nullability_name(
    MyliteCreateTableColumnNullability nullability);
const char *mylite_create_table_column_generated_storage_name(
    MyliteCreateTableColumnGeneratedStorage storage);
const char *mylite_create_table_column_value_kind_name(
    MyliteCreateTableColumnValueKind kind);
const char *mylite_create_table_key_kind_name(MyliteCreateTableKeyKind kind);
const char *mylite_create_table_key_part_kind_name(
    MyliteCreateTableKeyPartKind kind);
const char *mylite_create_table_key_part_order_name(
    MyliteCreateTableKeyPartOrder order);
const char *mylite_create_table_index_type_name(
    MyliteCreateTableIndexType type);
const char *mylite_create_table_key_visibility_name(
    MyliteCreateTableKeyVisibility visibility);
const char *mylite_create_table_key_option_kind_name(
    MyliteCreateTableKeyOptionKind kind);
const char *mylite_create_table_key_option_value_kind_name(
    MyliteCreateTableKeyOptionValueKind kind);
const char *mylite_create_table_foreign_match_kind_name(
    MyliteCreateTableForeignMatchKind kind);
const char *mylite_create_table_foreign_action_name(
    MyliteCreateTableForeignAction action);
const char *mylite_create_table_check_enforcement_name(
    MyliteCreateTableCheckEnforcement enforcement);
const char *mylite_create_table_option_kind_name(
    MyliteCreateTableOptionKind kind);
const char *mylite_create_table_option_value_kind_name(
    MyliteCreateTableOptionValueKind kind);
const char *mylite_alter_table_spec_kind_name(MyliteAlterTableSpecKind kind);

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
const char *mylite_ast_statement_target_schema_value(const MyliteAst *ast,
                                                     size_t index);
size_t mylite_ast_statement_target_schema_value_length(const MyliteAst *ast,
                                                       size_t index);
size_t mylite_ast_statement_target_name_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_name_end(const MyliteAst *ast, size_t index);
const char *mylite_ast_statement_target_name_value(const MyliteAst *ast,
                                                   size_t index);
size_t mylite_ast_statement_target_name_value_length(const MyliteAst *ast,
                                                     size_t index);
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
const char *mylite_ast_statement_target_schema_value_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index);
size_t mylite_ast_statement_target_schema_value_length_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index);
size_t mylite_ast_statement_target_name_start_at(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t target_index);
size_t mylite_ast_statement_target_name_end_at(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t target_index);
const char *mylite_ast_statement_target_name_value_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index);
size_t mylite_ast_statement_target_name_value_length_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index);
const MyliteAstAlterTable *mylite_ast_alter_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstCreateTable *mylite_ast_create_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstCreateIndex *mylite_ast_create_index_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstDropIndex *mylite_ast_drop_index_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstDropTable *mylite_ast_drop_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstRenameTable *mylite_ast_rename_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstTruncateTable *mylite_ast_truncate_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstNode *mylite_ast_alter_table_view_node(
    const MyliteAstAlterTable *alter_table);
size_t mylite_ast_alter_table_view_start(
    const MyliteAstAlterTable *alter_table);
size_t mylite_ast_alter_table_view_end(
    const MyliteAstAlterTable *alter_table);
size_t mylite_ast_alter_table_view_target_start(
    const MyliteAstAlterTable *alter_table);
size_t mylite_ast_alter_table_view_target_end(
    const MyliteAstAlterTable *alter_table);
const char *mylite_ast_alter_table_view_schema_value(
    const MyliteAstAlterTable *alter_table);
size_t mylite_ast_alter_table_view_schema_value_length(
    const MyliteAstAlterTable *alter_table);
const char *mylite_ast_alter_table_view_name_value(
    const MyliteAstAlterTable *alter_table);
size_t mylite_ast_alter_table_view_name_value_length(
    const MyliteAstAlterTable *alter_table);
size_t mylite_ast_alter_table_view_spec_count(
    const MyliteAstAlterTable *alter_table);
const MyliteAstAlterTableSpec *mylite_ast_alter_table_view_spec_at(
    const MyliteAstAlterTable *alter_table, size_t spec_index);
size_t mylite_ast_alter_table_view_option_count(
    const MyliteAstAlterTable *alter_table);
const MyliteAstCreateTableOption *mylite_ast_alter_table_view_option_at(
    const MyliteAstAlterTable *alter_table, size_t option_index);
const MyliteAstNode *mylite_ast_alter_table_spec_view_node(
    const MyliteAstAlterTableSpec *spec);
const MyliteAstCreateTableColumn *mylite_ast_alter_table_spec_view_column(
    const MyliteAstAlterTableSpec *spec);
const MyliteAstCreateTableKey *mylite_ast_alter_table_spec_view_key(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_column_count(
    const MyliteAstAlterTableSpec *spec);
const MyliteAstCreateTableColumn *mylite_ast_alter_table_spec_view_column_at(
    const MyliteAstAlterTableSpec *spec, size_t column_index);
size_t mylite_ast_alter_table_spec_view_key_count(
    const MyliteAstAlterTableSpec *spec);
const MyliteAstCreateTableKey *mylite_ast_alter_table_spec_view_key_at(
    const MyliteAstAlterTableSpec *spec, size_t key_index);
MyliteAlterTableSpecKind mylite_ast_alter_table_spec_view_kind(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_start(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_end(
    const MyliteAstAlterTableSpec *spec);
int mylite_ast_alter_table_spec_view_has_if_exists(
    const MyliteAstAlterTableSpec *spec);
int mylite_ast_alter_table_spec_view_has_if_not_exists(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_name_start(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_name_end(
    const MyliteAstAlterTableSpec *spec);
const char *mylite_ast_alter_table_spec_view_name_value(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_name_value_length(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_secondary_name_start(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_secondary_name_end(
    const MyliteAstAlterTableSpec *spec);
const char *mylite_ast_alter_table_spec_view_secondary_name_value(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_secondary_name_value_length(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_table_start(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_table_end(
    const MyliteAstAlterTableSpec *spec);
const char *mylite_ast_alter_table_spec_view_table_schema_value(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_table_schema_value_length(
    const MyliteAstAlterTableSpec *spec);
const char *mylite_ast_alter_table_spec_view_table_name_value(
    const MyliteAstAlterTableSpec *spec);
size_t mylite_ast_alter_table_spec_view_table_name_value_length(
    const MyliteAstAlterTableSpec *spec);
const MyliteAstNode *mylite_ast_create_table_view_node(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_start(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_end(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_target_start(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_target_end(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_schema_start(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_schema_end(
    const MyliteAstCreateTable *create_table);
const char *mylite_ast_create_table_view_schema_value(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_schema_value_length(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_name_start(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_name_end(
    const MyliteAstCreateTable *create_table);
const char *mylite_ast_create_table_view_name_value(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_name_value_length(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_column_count(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_key_count(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_option_count(
    const MyliteAstCreateTable *create_table);
const MyliteAstCreateTableOption *mylite_ast_create_table_view_engine_option(
    const MyliteAstCreateTable *create_table);
const char *mylite_ast_create_table_view_engine_value(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_engine_value_length(
    const MyliteAstCreateTable *create_table);
const MyliteAstCreateTableOption *mylite_ast_create_table_view_charset_option(
    const MyliteAstCreateTable *create_table);
const char *mylite_ast_create_table_view_charset_value(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_charset_value_length(
    const MyliteAstCreateTable *create_table);
const MyliteAstCreateTableOption *mylite_ast_create_table_view_collation_option(
    const MyliteAstCreateTable *create_table);
const char *mylite_ast_create_table_view_collation_value(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_collation_value_length(
    const MyliteAstCreateTable *create_table);
const MyliteAstCreateTableOption *mylite_ast_create_table_view_comment_option(
    const MyliteAstCreateTable *create_table);
const char *mylite_ast_create_table_view_comment_value(
    const MyliteAstCreateTable *create_table);
size_t mylite_ast_create_table_view_comment_value_length(
    const MyliteAstCreateTable *create_table);
const MyliteAstCreateTableOption *
mylite_ast_create_table_view_auto_increment_option(
    const MyliteAstCreateTable *create_table);
int mylite_ast_create_table_view_has_auto_increment_value(
    const MyliteAstCreateTable *create_table);
unsigned long long mylite_ast_create_table_view_auto_increment_value(
    const MyliteAstCreateTable *create_table);
const MyliteAstCreateTableColumn *mylite_ast_create_table_view_column_at(
    const MyliteAstCreateTable *create_table, size_t column_index);
const MyliteAstCreateTableKey *mylite_ast_create_table_view_key_at(
    const MyliteAstCreateTable *create_table, size_t key_index);
const MyliteAstCreateTableOption *mylite_ast_create_table_view_option_at(
    const MyliteAstCreateTable *create_table, size_t option_index);
const MyliteAstNode *mylite_ast_create_index_view_node(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_start(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_end(
    const MyliteAstCreateIndex *create_index);
MyliteCreateTableKeyKind mylite_ast_create_index_view_key_kind(
    const MyliteAstCreateIndex *create_index);
MyliteCreateTableIndexType mylite_ast_create_index_view_index_type_kind(
    const MyliteAstCreateIndex *create_index);
MyliteCreateTableKeyVisibility mylite_ast_create_index_view_visibility(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_name_start(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_name_end(
    const MyliteAstCreateIndex *create_index);
const char *mylite_ast_create_index_view_name_value(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_name_value_length(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_table_start(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_table_end(
    const MyliteAstCreateIndex *create_index);
const char *mylite_ast_create_index_view_table_schema_value(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_table_schema_value_length(
    const MyliteAstCreateIndex *create_index);
const char *mylite_ast_create_index_view_table_name_value(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_table_name_value_length(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_column_count(
    const MyliteAstCreateIndex *create_index);
const MyliteAstCreateTableKeyPart *mylite_ast_create_index_view_column_at(
    const MyliteAstCreateIndex *create_index, size_t column_index);
size_t mylite_ast_create_index_view_option_count(
    const MyliteAstCreateIndex *create_index);
const MyliteAstCreateTableKeyOption *mylite_ast_create_index_view_option_at(
    const MyliteAstCreateIndex *create_index, size_t option_index);
const char *mylite_ast_create_index_view_comment_value(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_comment_value_length(
    const MyliteAstCreateIndex *create_index);
const char *mylite_ast_create_index_view_parser_value(
    const MyliteAstCreateIndex *create_index);
size_t mylite_ast_create_index_view_parser_value_length(
    const MyliteAstCreateIndex *create_index);
int mylite_ast_create_index_view_has_key_block_size_value(
    const MyliteAstCreateIndex *create_index);
unsigned long long mylite_ast_create_index_view_key_block_size_value(
    const MyliteAstCreateIndex *create_index);
const MyliteAstNode *mylite_ast_drop_index_view_node(
    const MyliteAstDropIndex *drop_index);
size_t mylite_ast_drop_index_view_start(
    const MyliteAstDropIndex *drop_index);
size_t mylite_ast_drop_index_view_end(const MyliteAstDropIndex *drop_index);
int mylite_ast_drop_index_view_has_if_exists(
    const MyliteAstDropIndex *drop_index);
int mylite_ast_drop_index_view_is_hypothetical(
    const MyliteAstDropIndex *drop_index);
size_t mylite_ast_drop_index_view_name_start(
    const MyliteAstDropIndex *drop_index);
size_t mylite_ast_drop_index_view_name_end(
    const MyliteAstDropIndex *drop_index);
const char *mylite_ast_drop_index_view_name_value(
    const MyliteAstDropIndex *drop_index);
size_t mylite_ast_drop_index_view_name_value_length(
    const MyliteAstDropIndex *drop_index);
const char *mylite_ast_drop_index_view_table_schema_value(
    const MyliteAstDropIndex *drop_index);
size_t mylite_ast_drop_index_view_table_schema_value_length(
    const MyliteAstDropIndex *drop_index);
const char *mylite_ast_drop_index_view_table_name_value(
    const MyliteAstDropIndex *drop_index);
size_t mylite_ast_drop_index_view_table_name_value_length(
    const MyliteAstDropIndex *drop_index);
const MyliteAstNode *mylite_ast_drop_table_view_node(
    const MyliteAstDropTable *drop_table);
size_t mylite_ast_drop_table_view_start(const MyliteAstDropTable *drop_table);
size_t mylite_ast_drop_table_view_end(const MyliteAstDropTable *drop_table);
int mylite_ast_drop_table_view_is_temporary(
    const MyliteAstDropTable *drop_table);
int mylite_ast_drop_table_view_has_if_exists(
    const MyliteAstDropTable *drop_table);
size_t mylite_ast_drop_table_view_table_count(
    const MyliteAstDropTable *drop_table);
const char *mylite_ast_drop_table_view_table_schema_value_at(
    const MyliteAstDropTable *drop_table, size_t table_index);
size_t mylite_ast_drop_table_view_table_schema_value_length_at(
    const MyliteAstDropTable *drop_table, size_t table_index);
const char *mylite_ast_drop_table_view_table_name_value_at(
    const MyliteAstDropTable *drop_table, size_t table_index);
size_t mylite_ast_drop_table_view_table_name_value_length_at(
    const MyliteAstDropTable *drop_table, size_t table_index);
const MyliteAstNode *mylite_ast_rename_table_view_node(
    const MyliteAstRenameTable *rename_table);
size_t mylite_ast_rename_table_view_start(
    const MyliteAstRenameTable *rename_table);
size_t mylite_ast_rename_table_view_end(
    const MyliteAstRenameTable *rename_table);
size_t mylite_ast_rename_table_view_pair_count(
    const MyliteAstRenameTable *rename_table);
const char *mylite_ast_rename_table_view_source_schema_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
size_t mylite_ast_rename_table_view_source_schema_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
const char *mylite_ast_rename_table_view_source_name_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
size_t mylite_ast_rename_table_view_source_name_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
const char *mylite_ast_rename_table_view_destination_schema_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
size_t mylite_ast_rename_table_view_destination_schema_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
const char *mylite_ast_rename_table_view_destination_name_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
size_t mylite_ast_rename_table_view_destination_name_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index);
const MyliteAstNode *mylite_ast_truncate_table_view_node(
    const MyliteAstTruncateTable *truncate_table);
size_t mylite_ast_truncate_table_view_start(
    const MyliteAstTruncateTable *truncate_table);
size_t mylite_ast_truncate_table_view_end(
    const MyliteAstTruncateTable *truncate_table);
int mylite_ast_truncate_table_view_has_table_keyword(
    const MyliteAstTruncateTable *truncate_table);
const char *mylite_ast_truncate_table_view_schema_value(
    const MyliteAstTruncateTable *truncate_table);
size_t mylite_ast_truncate_table_view_schema_value_length(
    const MyliteAstTruncateTable *truncate_table);
const char *mylite_ast_truncate_table_view_name_value(
    const MyliteAstTruncateTable *truncate_table);
size_t mylite_ast_truncate_table_view_name_value_length(
    const MyliteAstTruncateTable *truncate_table);
size_t mylite_ast_create_table_column_view_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_end(
    const MyliteAstCreateTableColumn *column);
const char *mylite_ast_create_table_column_view_name_value(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_name_value_length(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableColumnTypeFamily mylite_ast_create_table_column_view_type_family(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableColumnTypeKind mylite_ast_create_table_column_view_type_kind(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableColumnStorageClass
mylite_ast_create_table_column_view_storage_class(
    const MyliteAstCreateTableColumn *column);
unsigned int mylite_ast_create_table_column_view_flags(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableColumnNullability
mylite_ast_create_table_column_view_nullability(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableColumnGeneratedStorage
mylite_ast_create_table_column_view_generated_storage_kind(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_name_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_name_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_type_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_name_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_name_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_parameters_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_parameters_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_numeric_parameter_count(
    const MyliteAstCreateTableColumn *column);
unsigned long long
mylite_ast_create_table_column_view_type_numeric_parameter_at(
    const MyliteAstCreateTableColumn *column, size_t parameter_index);
size_t mylite_ast_create_table_column_view_type_element_count(
    const MyliteAstCreateTableColumn *column);
const MyliteAstCreateTableColumnTypeElement *
mylite_ast_create_table_column_view_type_element_at(
    const MyliteAstCreateTableColumn *column, size_t element_index);
size_t mylite_ast_create_table_column_type_element_view_start(
    const MyliteAstCreateTableColumnTypeElement *element);
size_t mylite_ast_create_table_column_type_element_view_end(
    const MyliteAstCreateTableColumnTypeElement *element);
const char *mylite_ast_create_table_column_type_element_view_value(
    const MyliteAstCreateTableColumnTypeElement *element);
size_t mylite_ast_create_table_column_type_element_view_value_length(
    const MyliteAstCreateTableColumnTypeElement *element);
int mylite_ast_create_table_column_view_type_has_length(
    const MyliteAstCreateTableColumn *column);
unsigned long long mylite_ast_create_table_column_view_type_length(
    const MyliteAstCreateTableColumn *column);
int mylite_ast_create_table_column_view_type_has_precision(
    const MyliteAstCreateTableColumn *column);
unsigned long long mylite_ast_create_table_column_view_type_precision(
    const MyliteAstCreateTableColumn *column);
int mylite_ast_create_table_column_view_type_has_scale(
    const MyliteAstCreateTableColumn *column);
unsigned long long mylite_ast_create_table_column_view_type_scale(
    const MyliteAstCreateTableColumn *column);
int mylite_ast_create_table_column_view_type_has_fractional_seconds_precision(
    const MyliteAstCreateTableColumn *column);
unsigned long long
mylite_ast_create_table_column_view_type_fractional_seconds_precision(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_attributes_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_attributes_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_unsigned_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_unsigned_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_zerofill_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_zerofill_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_binary_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_binary_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_charset_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_charset_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_charset_value_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_charset_value_end(
    const MyliteAstCreateTableColumn *column);
const char *mylite_ast_create_table_column_view_type_charset_value(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_charset_value_length(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_collation_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_collation_end(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_collation_value_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_collation_value_end(
    const MyliteAstCreateTableColumn *column);
const char *mylite_ast_create_table_column_view_type_collation_value(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_type_collation_value_length(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_options_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_options_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_options_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_default_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_default_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_default_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_default_value_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_default_value_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_default_value_node(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableColumnValueKind
mylite_ast_create_table_column_view_default_value_kind(
    const MyliteAstCreateTableColumn *column);
const char *mylite_ast_create_table_column_view_default_value(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_default_value_length(
    const MyliteAstCreateTableColumn *column);
int mylite_ast_create_table_column_view_has_default_unsigned_integer(
    const MyliteAstCreateTableColumn *column);
unsigned long long
mylite_ast_create_table_column_view_default_unsigned_integer_value(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_on_update_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_on_update_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_on_update_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_on_update_value_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_on_update_value_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *
mylite_ast_create_table_column_view_on_update_value_node(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableColumnValueKind
mylite_ast_create_table_column_view_on_update_value_kind(
    const MyliteAstCreateTableColumn *column);
const char *mylite_ast_create_table_column_view_on_update_value(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_on_update_value_length(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_generated_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_generated_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_generated_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_generated_expression_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_generated_expression_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *
mylite_ast_create_table_column_view_generated_expression_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_generated_storage_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_generated_storage_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *
mylite_ast_create_table_column_view_generated_storage_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_comment_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_comment_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_comment_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_comment_value_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_comment_value_end(
    const MyliteAstCreateTableColumn *column);
const char *mylite_ast_create_table_column_view_comment_value(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_comment_value_length(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_check_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_check_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_check_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_check_expression_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_check_expression_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *
mylite_ast_create_table_column_view_check_expression_node(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableCheckEnforcement
mylite_ast_create_table_column_view_check_enforcement(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_check_enforcement_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_check_enforcement_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *
mylite_ast_create_table_column_view_check_enforcement_node(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_reference_start(
    const MyliteAstCreateTableColumn *column);
size_t mylite_ast_create_table_column_view_reference_end(
    const MyliteAstCreateTableColumn *column);
const MyliteAstNode *mylite_ast_create_table_column_view_reference_node(
    const MyliteAstCreateTableColumn *column);
MyliteCreateTableKeyKind mylite_ast_create_table_key_view_kind(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_end(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_constraint_name_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_constraint_name_end(
    const MyliteAstCreateTableKey *key);
const char *mylite_ast_create_table_key_view_constraint_name_value(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_constraint_name_value_length(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_name_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_name_end(
    const MyliteAstCreateTableKey *key);
const char *mylite_ast_create_table_key_view_name_value(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_name_value_length(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_index_type_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_index_type_end(
    const MyliteAstCreateTableKey *key);
MyliteCreateTableIndexType mylite_ast_create_table_key_view_index_type_kind(
    const MyliteAstCreateTableKey *key);
MyliteCreateTableKeyVisibility mylite_ast_create_table_key_view_visibility(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_column_count(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_column_count(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_option_count(
    const MyliteAstCreateTableKey *key);
const MyliteAstCreateTableKeyPart *mylite_ast_create_table_key_view_column_at(
    const MyliteAstCreateTableKey *key, size_t column_index);
size_t mylite_ast_create_table_key_view_referenced_table_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_table_end(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_table_schema_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_table_schema_end(
    const MyliteAstCreateTableKey *key);
const char *mylite_ast_create_table_key_view_referenced_table_schema_value(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_table_schema_value_length(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_table_name_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_table_name_end(
    const MyliteAstCreateTableKey *key);
const char *mylite_ast_create_table_key_view_referenced_table_name_value(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_referenced_table_name_value_length(
    const MyliteAstCreateTableKey *key);
const MyliteAstCreateTableKeyPart *
mylite_ast_create_table_key_view_referenced_column_at(
    const MyliteAstCreateTableKey *key, size_t column_index);
MyliteCreateTableForeignMatchKind
mylite_ast_create_table_key_view_foreign_match_kind(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_foreign_match_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_foreign_match_end(
    const MyliteAstCreateTableKey *key);
MyliteCreateTableForeignAction
mylite_ast_create_table_key_view_foreign_on_delete_action(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_foreign_on_delete_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_foreign_on_delete_end(
    const MyliteAstCreateTableKey *key);
MyliteCreateTableForeignAction
mylite_ast_create_table_key_view_foreign_on_update_action(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_foreign_on_update_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_foreign_on_update_end(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_check_expression_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_check_expression_end(
    const MyliteAstCreateTableKey *key);
MyliteCreateTableCheckEnforcement
mylite_ast_create_table_key_view_check_enforcement(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_check_enforcement_start(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_check_enforcement_end(
    const MyliteAstCreateTableKey *key);
const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_option_at(
    const MyliteAstCreateTableKey *key, size_t option_index);
const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_comment_option(
    const MyliteAstCreateTableKey *key);
const char *mylite_ast_create_table_key_view_comment_value(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_comment_value_length(
    const MyliteAstCreateTableKey *key);
const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_parser_option(
    const MyliteAstCreateTableKey *key);
const char *mylite_ast_create_table_key_view_parser_value(
    const MyliteAstCreateTableKey *key);
size_t mylite_ast_create_table_key_view_parser_value_length(
    const MyliteAstCreateTableKey *key);
const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_key_block_size_option(
    const MyliteAstCreateTableKey *key);
int mylite_ast_create_table_key_view_has_key_block_size_value(
    const MyliteAstCreateTableKey *key);
unsigned long long mylite_ast_create_table_key_view_key_block_size_value(
    const MyliteAstCreateTableKey *key);
MyliteCreateTableKeyPartKind mylite_ast_create_table_key_part_view_kind(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_start(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_end(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_name_start(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_name_end(
    const MyliteAstCreateTableKeyPart *part);
const char *mylite_ast_create_table_key_part_view_name_value(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_name_value_length(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_expression_start(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_expression_end(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_prefix_start(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_prefix_end(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_prefix_value_start(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_prefix_value_end(
    const MyliteAstCreateTableKeyPart *part);
MyliteCreateTableKeyPartOrder mylite_ast_create_table_key_part_view_order(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_order_start(
    const MyliteAstCreateTableKeyPart *part);
size_t mylite_ast_create_table_key_part_view_order_end(
    const MyliteAstCreateTableKeyPart *part);
MyliteCreateTableKeyOptionKind mylite_ast_create_table_key_option_view_kind(
    const MyliteAstCreateTableKeyOption *option);
size_t mylite_ast_create_table_key_option_view_start(
    const MyliteAstCreateTableKeyOption *option);
size_t mylite_ast_create_table_key_option_view_end(
    const MyliteAstCreateTableKeyOption *option);
size_t mylite_ast_create_table_key_option_view_name_start(
    const MyliteAstCreateTableKeyOption *option);
size_t mylite_ast_create_table_key_option_view_name_end(
    const MyliteAstCreateTableKeyOption *option);
size_t mylite_ast_create_table_key_option_view_value_start(
    const MyliteAstCreateTableKeyOption *option);
size_t mylite_ast_create_table_key_option_view_value_end(
    const MyliteAstCreateTableKeyOption *option);
MyliteCreateTableKeyOptionValueKind
mylite_ast_create_table_key_option_view_value_kind(
    const MyliteAstCreateTableKeyOption *option);
const char *mylite_ast_create_table_key_option_view_value(
    const MyliteAstCreateTableKeyOption *option);
size_t mylite_ast_create_table_key_option_view_value_length(
    const MyliteAstCreateTableKeyOption *option);
int mylite_ast_create_table_key_option_view_has_unsigned_integer(
    const MyliteAstCreateTableKeyOption *option);
unsigned long long
mylite_ast_create_table_key_option_view_unsigned_integer_value(
    const MyliteAstCreateTableKeyOption *option);
MyliteCreateTableIndexType
mylite_ast_create_table_key_option_view_index_type_kind(
    const MyliteAstCreateTableKeyOption *option);
MyliteCreateTableOptionKind mylite_ast_create_table_option_view_kind(
    const MyliteAstCreateTableOption *option);
size_t mylite_ast_create_table_option_view_start(
    const MyliteAstCreateTableOption *option);
size_t mylite_ast_create_table_option_view_end(
    const MyliteAstCreateTableOption *option);
size_t mylite_ast_create_table_option_view_name_start(
    const MyliteAstCreateTableOption *option);
size_t mylite_ast_create_table_option_view_name_end(
    const MyliteAstCreateTableOption *option);
size_t mylite_ast_create_table_option_view_value_start(
    const MyliteAstCreateTableOption *option);
size_t mylite_ast_create_table_option_view_value_end(
    const MyliteAstCreateTableOption *option);
MyliteCreateTableOptionValueKind
mylite_ast_create_table_option_view_value_kind(
    const MyliteAstCreateTableOption *option);
const char *mylite_ast_create_table_option_view_value(
    const MyliteAstCreateTableOption *option);
size_t mylite_ast_create_table_option_view_value_length(
    const MyliteAstCreateTableOption *option);
int mylite_ast_create_table_option_view_has_unsigned_integer(
    const MyliteAstCreateTableOption *option);
unsigned long long
mylite_ast_create_table_option_view_unsigned_integer_value(
    const MyliteAstCreateTableOption *option);
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
const char *mylite_ast_create_table_column_name_value(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index);
size_t mylite_ast_create_table_column_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
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
size_t mylite_ast_create_table_column_type_element_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index);
size_t mylite_ast_create_table_column_type_element_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index);
const char *mylite_ast_create_table_column_type_element_value(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index);
size_t mylite_ast_create_table_column_type_element_value_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index);
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
size_t mylite_ast_create_table_column_type_unsigned_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_unsigned_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_zerofill_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_zerofill_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_binary_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_binary_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_charset_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_charset_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_charset_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_charset_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_collation_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_collation_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_collation_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
size_t mylite_ast_create_table_column_type_collation_value_end(
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
const char *mylite_ast_create_table_key_constraint_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_constraint_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_name_start(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index);
size_t mylite_ast_create_table_key_name_end(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t key_index);
const char *mylite_ast_create_table_key_name_value(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t key_index);
size_t mylite_ast_create_table_key_name_value_length(const MyliteAst *ast,
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
const char *mylite_ast_create_table_key_column_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_column_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
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
const char *mylite_ast_create_table_key_referenced_table_schema_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_schema_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_name_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_name_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
const char *mylite_ast_create_table_key_referenced_table_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
size_t mylite_ast_create_table_key_referenced_table_name_value_length(
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
const char *mylite_ast_create_table_key_referenced_column_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index);
size_t mylite_ast_create_table_key_referenced_column_name_value_length(
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
