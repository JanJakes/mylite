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
typedef struct MyliteAstCreateDatabase MyliteAstCreateDatabase;
typedef struct MyliteAstCreateIndex MyliteAstCreateIndex;
typedef struct MyliteAstCreateTable MyliteAstCreateTable;
typedef struct MyliteAstCreateTableColumn MyliteAstCreateTableColumn;
typedef struct MyliteAstCreateTableColumnTypeElement
    MyliteAstCreateTableColumnTypeElement;
typedef struct MyliteAstCreateTableKey MyliteAstCreateTableKey;
typedef struct MyliteAstCreateTableKeyPart MyliteAstCreateTableKeyPart;
typedef struct MyliteAstCreateTableKeyOption MyliteAstCreateTableKeyOption;
typedef struct MyliteAstCreateTableOption MyliteAstCreateTableOption;
typedef struct MyliteAstDatabaseOption MyliteAstDatabaseOption;
typedef struct MyliteAstDropDatabase MyliteAstDropDatabase;
typedef struct MyliteAstDropIndex MyliteAstDropIndex;
typedef struct MyliteAstDropTable MyliteAstDropTable;
typedef struct MyliteAstExpression MyliteAstExpression;
typedef struct MyliteAstDeallocateStatement MyliteAstDeallocateStatement;
typedef struct MyliteAstExecuteStatement MyliteAstExecuteStatement;
typedef struct MyliteAstInsertAssignment MyliteAstInsertAssignment;
typedef struct MyliteAstInsertColumn MyliteAstInsertColumn;
typedef struct MyliteAstInsertStatement MyliteAstInsertStatement;
typedef struct MyliteAstInsertValue MyliteAstInsertValue;
typedef struct MyliteAstNode MyliteAstNode;
typedef struct MyliteAstPreparedStatementVariable
    MyliteAstPreparedStatementVariable;
typedef struct MyliteAstPrepareStatement MyliteAstPrepareStatement;
typedef struct MyliteAstRenameTable MyliteAstRenameTable;
typedef struct MyliteAstSelectProjection MyliteAstSelectProjection;
typedef struct MyliteAstSelectStatement MyliteAstSelectStatement;
typedef struct MyliteAstSetAssignment MyliteAstSetAssignment;
typedef struct MyliteAstSetStatement MyliteAstSetStatement;
typedef struct MyliteAstTruncateTable MyliteAstTruncateTable;
typedef struct MyliteAstTransactionStatement MyliteAstTransactionStatement;
typedef struct MyliteAstUpdateAssignment MyliteAstUpdateAssignment;
typedef struct MyliteAstUpdateStatement MyliteAstUpdateStatement;
typedef struct MyliteAstUseDatabase MyliteAstUseDatabase;
typedef struct MyliteAstCreateView MyliteAstCreateView;
typedef struct MyliteAstDropView MyliteAstDropView;
typedef struct MyliteAstViewColumn MyliteAstViewColumn;

typedef enum MyliteAstNodeKind {
  MYLITE_AST_NODE_RULE = 1,
  MYLITE_AST_NODE_TOKEN = 2
} MyliteAstNodeKind;

typedef enum MyliteExpressionKind {
  MYLITE_EXPRESSION_UNKNOWN = 0,
  MYLITE_EXPRESSION_RAW,
  MYLITE_EXPRESSION_LITERAL,
  MYLITE_EXPRESSION_IDENTIFIER,
  MYLITE_EXPRESSION_VARIABLE,
  MYLITE_EXPRESSION_FUNCTION_CALL,
  MYLITE_EXPRESSION_DEFAULT,
  MYLITE_EXPRESSION_PARAMETER,
  MYLITE_EXPRESSION_UNARY,
  MYLITE_EXPRESSION_BINARY,
  MYLITE_EXPRESSION_PARENTHESIZED
} MyliteExpressionKind;

typedef enum MyliteExpressionLiteralKind {
  MYLITE_EXPRESSION_LITERAL_NONE = 0,
  MYLITE_EXPRESSION_LITERAL_STRING,
  MYLITE_EXPRESSION_LITERAL_UNSIGNED_INTEGER,
  MYLITE_EXPRESSION_LITERAL_FLOAT,
  MYLITE_EXPRESSION_LITERAL_HEX,
  MYLITE_EXPRESSION_LITERAL_BIT,
  MYLITE_EXPRESSION_LITERAL_NULL,
  MYLITE_EXPRESSION_LITERAL_TRUE,
  MYLITE_EXPRESSION_LITERAL_FALSE
} MyliteExpressionLiteralKind;

typedef enum MyliteExpressionOperatorKind {
  MYLITE_EXPRESSION_OPERATOR_NONE = 0,
  MYLITE_EXPRESSION_OPERATOR_ASSIGNMENT,
  MYLITE_EXPRESSION_OPERATOR_LOGICAL_OR,
  MYLITE_EXPRESSION_OPERATOR_LOGICAL_XOR,
  MYLITE_EXPRESSION_OPERATOR_LOGICAL_AND,
  MYLITE_EXPRESSION_OPERATOR_IS,
  MYLITE_EXPRESSION_OPERATOR_IS_NOT,
  MYLITE_EXPRESSION_OPERATOR_EQ,
  MYLITE_EXPRESSION_OPERATOR_NULL_SAFE_EQ,
  MYLITE_EXPRESSION_OPERATOR_NEQ,
  MYLITE_EXPRESSION_OPERATOR_LT,
  MYLITE_EXPRESSION_OPERATOR_LE,
  MYLITE_EXPRESSION_OPERATOR_GT,
  MYLITE_EXPRESSION_OPERATOR_GE,
  MYLITE_EXPRESSION_OPERATOR_BIT_OR,
  MYLITE_EXPRESSION_OPERATOR_BIT_AND,
  MYLITE_EXPRESSION_OPERATOR_LEFT_SHIFT,
  MYLITE_EXPRESSION_OPERATOR_RIGHT_SHIFT,
  MYLITE_EXPRESSION_OPERATOR_ADD,
  MYLITE_EXPRESSION_OPERATOR_SUBTRACT,
  MYLITE_EXPRESSION_OPERATOR_MULTIPLY,
  MYLITE_EXPRESSION_OPERATOR_DIVIDE,
  MYLITE_EXPRESSION_OPERATOR_INTEGER_DIVIDE,
  MYLITE_EXPRESSION_OPERATOR_MODULO,
  MYLITE_EXPRESSION_OPERATOR_BIT_XOR,
  MYLITE_EXPRESSION_OPERATOR_CONCAT,
  MYLITE_EXPRESSION_OPERATOR_COLLATE,
  MYLITE_EXPRESSION_OPERATOR_UNARY_LOGICAL_NOT,
  MYLITE_EXPRESSION_OPERATOR_UNARY_BIT_NOT,
  MYLITE_EXPRESSION_OPERATOR_UNARY_MINUS,
  MYLITE_EXPRESSION_OPERATOR_UNARY_PLUS,
  MYLITE_EXPRESSION_OPERATOR_UNARY_BINARY
} MyliteExpressionOperatorKind;

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
  MYLITE_STATEMENT_USE,
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

typedef enum MyliteSelectProjectionKind {
  MYLITE_SELECT_PROJECTION_UNKNOWN = 0,
  MYLITE_SELECT_PROJECTION_EXPRESSION,
  MYLITE_SELECT_PROJECTION_WILDCARD,
  MYLITE_SELECT_PROJECTION_TABLE_WILDCARD
} MyliteSelectProjectionKind;

typedef enum MyliteInsertSourceKind {
  MYLITE_INSERT_SOURCE_UNKNOWN = 0,
  MYLITE_INSERT_SOURCE_VALUES,
  MYLITE_INSERT_SOURCE_SET,
  MYLITE_INSERT_SOURCE_SELECT
} MyliteInsertSourceKind;

typedef enum MyliteInsertPriority {
  MYLITE_INSERT_PRIORITY_NONE = 0,
  MYLITE_INSERT_PRIORITY_LOW,
  MYLITE_INSERT_PRIORITY_HIGH,
  MYLITE_INSERT_PRIORITY_DELAYED
} MyliteInsertPriority;

typedef enum MyliteUpdatePriority {
  MYLITE_UPDATE_PRIORITY_NONE = 0,
  MYLITE_UPDATE_PRIORITY_LOW,
  MYLITE_UPDATE_PRIORITY_HIGH,
  MYLITE_UPDATE_PRIORITY_DELAYED
} MyliteUpdatePriority;

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

typedef enum MyliteDatabaseOptionKind {
  MYLITE_DATABASE_OPTION_UNKNOWN = 0,
  MYLITE_DATABASE_OPTION_CHARSET,
  MYLITE_DATABASE_OPTION_COLLATE,
  MYLITE_DATABASE_OPTION_ENCRYPTION,
  MYLITE_DATABASE_OPTION_PLACEMENT_POLICY,
  MYLITE_DATABASE_OPTION_TI_FLASH_REPLICA,
  MYLITE_DATABASE_OPTION_READ_ONLY
} MyliteDatabaseOptionKind;

typedef enum MyliteDatabaseOptionValueKind {
  MYLITE_DATABASE_OPTION_VALUE_UNKNOWN = 0,
  MYLITE_DATABASE_OPTION_VALUE_RAW,
  MYLITE_DATABASE_OPTION_VALUE_IDENTIFIER,
  MYLITE_DATABASE_OPTION_VALUE_STRING,
  MYLITE_DATABASE_OPTION_VALUE_DEFAULT
} MyliteDatabaseOptionValueKind;

typedef enum MyliteCreateViewAlgorithm {
  MYLITE_CREATE_VIEW_ALGORITHM_UNSPECIFIED = 0,
  MYLITE_CREATE_VIEW_ALGORITHM_UNDEFINED,
  MYLITE_CREATE_VIEW_ALGORITHM_MERGE,
  MYLITE_CREATE_VIEW_ALGORITHM_TEMPTABLE
} MyliteCreateViewAlgorithm;

typedef enum MyliteViewSqlSecurity {
  MYLITE_VIEW_SQL_SECURITY_UNSPECIFIED = 0,
  MYLITE_VIEW_SQL_SECURITY_DEFINER,
  MYLITE_VIEW_SQL_SECURITY_INVOKER
} MyliteViewSqlSecurity;

typedef enum MyliteViewCheckOption {
  MYLITE_VIEW_CHECK_OPTION_NONE = 0,
  MYLITE_VIEW_CHECK_OPTION_CASCADED,
  MYLITE_VIEW_CHECK_OPTION_LOCAL
} MyliteViewCheckOption;

typedef enum MyliteDropViewMode {
  MYLITE_DROP_VIEW_MODE_UNSPECIFIED = 0,
  MYLITE_DROP_VIEW_MODE_RESTRICT,
  MYLITE_DROP_VIEW_MODE_CASCADE
} MyliteDropViewMode;

typedef enum MyliteSetStatementForm {
  MYLITE_SET_STATEMENT_UNKNOWN = 0,
  MYLITE_SET_STATEMENT_ASSIGNMENTS,
  MYLITE_SET_STATEMENT_PASSWORD,
  MYLITE_SET_STATEMENT_TRANSACTION,
  MYLITE_SET_STATEMENT_CONFIG,
  MYLITE_SET_STATEMENT_SESSION_STATES,
  MYLITE_SET_STATEMENT_RESOURCE_GROUP,
  MYLITE_SET_STATEMENT_ROLE,
  MYLITE_SET_STATEMENT_DEFAULT_ROLE
} MyliteSetStatementForm;

typedef enum MyliteSetAssignmentKind {
  MYLITE_SET_ASSIGNMENT_UNKNOWN = 0,
  MYLITE_SET_ASSIGNMENT_SYSTEM_VARIABLE,
  MYLITE_SET_ASSIGNMENT_USER_VARIABLE,
  MYLITE_SET_ASSIGNMENT_NAMES,
  MYLITE_SET_ASSIGNMENT_CHARACTER_SET,
  MYLITE_SET_ASSIGNMENT_TRANSACTION_CHARACTERISTIC,
  MYLITE_SET_ASSIGNMENT_CONFIG
} MyliteSetAssignmentKind;

typedef enum MyliteSetVariableScope {
  MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED = 0,
  MYLITE_SET_VARIABLE_SCOPE_GLOBAL,
  MYLITE_SET_VARIABLE_SCOPE_SESSION,
  MYLITE_SET_VARIABLE_SCOPE_LOCAL,
  MYLITE_SET_VARIABLE_SCOPE_INSTANCE
} MyliteSetVariableScope;

typedef enum MyliteSetAssignmentOperator {
  MYLITE_SET_ASSIGNMENT_OPERATOR_NONE = 0,
  MYLITE_SET_ASSIGNMENT_OPERATOR_EQ,
  MYLITE_SET_ASSIGNMENT_OPERATOR_ASSIGNMENT_EQ
} MyliteSetAssignmentOperator;

typedef enum MylitePrepareStatementSourceKind {
  MYLITE_PREPARE_STATEMENT_SOURCE_UNKNOWN = 0,
  MYLITE_PREPARE_STATEMENT_SOURCE_STRING,
  MYLITE_PREPARE_STATEMENT_SOURCE_USER_VARIABLE
} MylitePrepareStatementSourceKind;

typedef enum MyliteDeallocateStatementMode {
  MYLITE_DEALLOCATE_STATEMENT_MODE_UNKNOWN = 0,
  MYLITE_DEALLOCATE_STATEMENT_MODE_DEALLOCATE,
  MYLITE_DEALLOCATE_STATEMENT_MODE_DROP
} MyliteDeallocateStatementMode;

typedef enum MyliteTransactionStatementKind {
  MYLITE_TRANSACTION_STATEMENT_UNKNOWN = 0,
  MYLITE_TRANSACTION_STATEMENT_BEGIN,
  MYLITE_TRANSACTION_STATEMENT_COMMIT,
  MYLITE_TRANSACTION_STATEMENT_ROLLBACK,
  MYLITE_TRANSACTION_STATEMENT_SAVEPOINT,
  MYLITE_TRANSACTION_STATEMENT_RELEASE_SAVEPOINT
} MyliteTransactionStatementKind;

typedef enum MyliteTransactionBeginForm {
  MYLITE_TRANSACTION_BEGIN_FORM_UNKNOWN = 0,
  MYLITE_TRANSACTION_BEGIN_FORM_BEGIN,
  MYLITE_TRANSACTION_BEGIN_FORM_START_TRANSACTION
} MyliteTransactionBeginForm;

typedef enum MyliteTransactionBeginMode {
  MYLITE_TRANSACTION_BEGIN_MODE_UNSPECIFIED = 0,
  MYLITE_TRANSACTION_BEGIN_MODE_PESSIMISTIC,
  MYLITE_TRANSACTION_BEGIN_MODE_OPTIMISTIC
} MyliteTransactionBeginMode;

typedef enum MyliteTransactionAccessMode {
  MYLITE_TRANSACTION_ACCESS_UNSPECIFIED = 0,
  MYLITE_TRANSACTION_ACCESS_READ_WRITE,
  MYLITE_TRANSACTION_ACCESS_READ_ONLY
} MyliteTransactionAccessMode;

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
const char *mylite_select_projection_kind_name(
    MyliteSelectProjectionKind kind);
const char *mylite_insert_source_kind_name(MyliteInsertSourceKind kind);
const char *mylite_insert_priority_name(MyliteInsertPriority priority);
const char *mylite_update_priority_name(MyliteUpdatePriority priority);
const char *mylite_expression_kind_name(MyliteExpressionKind kind);
const char *mylite_expression_literal_kind_name(
    MyliteExpressionLiteralKind kind);
const char *mylite_expression_operator_kind_name(
    MyliteExpressionOperatorKind kind);
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
const char *mylite_database_option_kind_name(MyliteDatabaseOptionKind kind);
const char *mylite_database_option_value_kind_name(
    MyliteDatabaseOptionValueKind kind);
const char *mylite_create_view_algorithm_name(MyliteCreateViewAlgorithm algorithm);
const char *mylite_view_sql_security_name(MyliteViewSqlSecurity security);
const char *mylite_view_check_option_name(MyliteViewCheckOption check_option);
const char *mylite_drop_view_mode_name(MyliteDropViewMode mode);
const char *mylite_set_statement_form_name(MyliteSetStatementForm form);
const char *mylite_set_assignment_kind_name(MyliteSetAssignmentKind kind);
const char *mylite_set_variable_scope_name(MyliteSetVariableScope scope);
const char *mylite_set_assignment_operator_name(
    MyliteSetAssignmentOperator operator_kind);
const char *mylite_prepare_statement_source_kind_name(
    MylitePrepareStatementSourceKind kind);
const char *mylite_deallocate_statement_mode_name(
    MyliteDeallocateStatementMode mode);
const char *mylite_transaction_statement_kind_name(
    MyliteTransactionStatementKind kind);
const char *mylite_transaction_begin_form_name(
    MyliteTransactionBeginForm form);
const char *mylite_transaction_begin_mode_name(
    MyliteTransactionBeginMode mode);
const char *mylite_transaction_access_mode_name(
    MyliteTransactionAccessMode mode);
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
const MyliteAstCreateDatabase *mylite_ast_create_database_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstCreateTable *mylite_ast_create_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstCreateIndex *mylite_ast_create_index_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstCreateView *mylite_ast_create_view_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstDropDatabase *mylite_ast_drop_database_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstDropIndex *mylite_ast_drop_index_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstDropTable *mylite_ast_drop_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstDropView *mylite_ast_drop_view_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstInsertStatement *mylite_ast_insert_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstPrepareStatement *mylite_ast_prepare_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstExecuteStatement *mylite_ast_execute_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstDeallocateStatement *mylite_ast_deallocate_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstRenameTable *mylite_ast_rename_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstSelectStatement *mylite_ast_select_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstSetStatement *mylite_ast_set_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstTruncateTable *mylite_ast_truncate_table_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstTransactionStatement *mylite_ast_transaction_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstUpdateStatement *mylite_ast_update_statement_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstUseDatabase *mylite_ast_use_database_view(
    const MyliteAst *ast, size_t statement_index);
const MyliteAstNode *mylite_ast_insert_statement_view_node(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_start(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_end(
    const MyliteAstInsertStatement *insert_statement);
MyliteInsertSourceKind mylite_ast_insert_statement_view_source_kind(
    const MyliteAstInsertStatement *insert_statement);
MyliteInsertPriority mylite_ast_insert_statement_view_priority(
    const MyliteAstInsertStatement *insert_statement);
int mylite_ast_insert_statement_view_has_ignore(
    const MyliteAstInsertStatement *insert_statement);
int mylite_ast_insert_statement_view_has_into(
    const MyliteAstInsertStatement *insert_statement);
int mylite_ast_insert_statement_view_has_partition_clause(
    const MyliteAstInsertStatement *insert_statement);
int mylite_ast_insert_statement_view_has_on_duplicate_key_update(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_start(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_end(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_schema_start(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_schema_end(
    const MyliteAstInsertStatement *insert_statement);
const char *mylite_ast_insert_statement_view_target_schema_value(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_schema_value_length(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_name_start(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_name_end(
    const MyliteAstInsertStatement *insert_statement);
const char *mylite_ast_insert_statement_view_target_name_value(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_target_name_value_length(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_partition_start(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_partition_end(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_source_start(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_source_end(
    const MyliteAstInsertStatement *insert_statement);
const MyliteAstNode *mylite_ast_insert_statement_view_source_node(
    const MyliteAstInsertStatement *insert_statement);
const MyliteAstNode *mylite_ast_insert_statement_view_select_source_node(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_column_count(
    const MyliteAstInsertStatement *insert_statement);
const MyliteAstInsertColumn *mylite_ast_insert_statement_view_column_at(
    const MyliteAstInsertStatement *insert_statement, size_t column_index);
size_t mylite_ast_insert_statement_view_value_row_count(
    const MyliteAstInsertStatement *insert_statement);
size_t mylite_ast_insert_statement_view_value_count(
    const MyliteAstInsertStatement *insert_statement);
const MyliteAstInsertValue *mylite_ast_insert_statement_view_value_at(
    const MyliteAstInsertStatement *insert_statement, size_t value_index);
size_t mylite_ast_insert_statement_view_set_assignment_count(
    const MyliteAstInsertStatement *insert_statement);
const MyliteAstInsertAssignment *
mylite_ast_insert_statement_view_set_assignment_at(
    const MyliteAstInsertStatement *insert_statement, size_t assignment_index);
size_t mylite_ast_insert_statement_view_duplicate_assignment_count(
    const MyliteAstInsertStatement *insert_statement);
const MyliteAstInsertAssignment *
mylite_ast_insert_statement_view_duplicate_assignment_at(
    const MyliteAstInsertStatement *insert_statement, size_t assignment_index);
const MyliteAstNode *mylite_ast_insert_column_view_node(
    const MyliteAstInsertColumn *column);
size_t mylite_ast_insert_column_view_start(
    const MyliteAstInsertColumn *column);
size_t mylite_ast_insert_column_view_end(
    const MyliteAstInsertColumn *column);
const char *mylite_ast_insert_column_view_name_value(
    const MyliteAstInsertColumn *column);
size_t mylite_ast_insert_column_view_name_value_length(
    const MyliteAstInsertColumn *column);
const MyliteAstNode *mylite_ast_insert_value_view_node(
    const MyliteAstInsertValue *value);
size_t mylite_ast_insert_value_view_row_index(
    const MyliteAstInsertValue *value);
size_t mylite_ast_insert_value_view_value_index(
    const MyliteAstInsertValue *value);
size_t mylite_ast_insert_value_view_start(
    const MyliteAstInsertValue *value);
size_t mylite_ast_insert_value_view_end(const MyliteAstInsertValue *value);
int mylite_ast_insert_value_view_is_default(
    const MyliteAstInsertValue *value);
const MyliteAstExpression *mylite_ast_insert_value_view_expression(
    const MyliteAstInsertValue *value);
const MyliteAstNode *mylite_ast_insert_assignment_view_node(
    const MyliteAstInsertAssignment *assignment);
size_t mylite_ast_insert_assignment_view_start(
    const MyliteAstInsertAssignment *assignment);
size_t mylite_ast_insert_assignment_view_end(
    const MyliteAstInsertAssignment *assignment);
size_t mylite_ast_insert_assignment_view_name_start(
    const MyliteAstInsertAssignment *assignment);
size_t mylite_ast_insert_assignment_view_name_end(
    const MyliteAstInsertAssignment *assignment);
const char *mylite_ast_insert_assignment_view_name_value(
    const MyliteAstInsertAssignment *assignment);
size_t mylite_ast_insert_assignment_view_name_value_length(
    const MyliteAstInsertAssignment *assignment);
const MyliteAstNode *mylite_ast_insert_assignment_view_value_node(
    const MyliteAstInsertAssignment *assignment);
size_t mylite_ast_insert_assignment_view_value_start(
    const MyliteAstInsertAssignment *assignment);
size_t mylite_ast_insert_assignment_view_value_end(
    const MyliteAstInsertAssignment *assignment);
const MyliteAstExpression *
mylite_ast_insert_assignment_view_value_expression(
    const MyliteAstInsertAssignment *assignment);
const MyliteAstNode *mylite_ast_update_statement_view_node(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_start(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_end(
    const MyliteAstUpdateStatement *update_statement);
int mylite_ast_update_statement_view_has_with_clause(
    const MyliteAstUpdateStatement *update_statement);
int mylite_ast_update_statement_view_has_ignore(
    const MyliteAstUpdateStatement *update_statement);
int mylite_ast_update_statement_view_is_multi_table(
    const MyliteAstUpdateStatement *update_statement);
MyliteUpdatePriority mylite_ast_update_statement_view_priority(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_table_reference_start(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_table_reference_end(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_assignment_count(
    const MyliteAstUpdateStatement *update_statement);
const MyliteAstUpdateAssignment *
mylite_ast_update_statement_view_assignment_at(
    const MyliteAstUpdateStatement *update_statement, size_t assignment_index);
size_t mylite_ast_update_statement_view_where_start(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_where_end(
    const MyliteAstUpdateStatement *update_statement);
const MyliteAstExpression *mylite_ast_update_statement_view_where_expression(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_order_by_start(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_order_by_end(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_limit_start(
    const MyliteAstUpdateStatement *update_statement);
size_t mylite_ast_update_statement_view_limit_end(
    const MyliteAstUpdateStatement *update_statement);
const MyliteAstNode *mylite_ast_update_assignment_view_node(
    const MyliteAstUpdateAssignment *assignment);
size_t mylite_ast_update_assignment_view_start(
    const MyliteAstUpdateAssignment *assignment);
size_t mylite_ast_update_assignment_view_end(
    const MyliteAstUpdateAssignment *assignment);
size_t mylite_ast_update_assignment_view_name_start(
    const MyliteAstUpdateAssignment *assignment);
size_t mylite_ast_update_assignment_view_name_end(
    const MyliteAstUpdateAssignment *assignment);
const char *mylite_ast_update_assignment_view_name_value(
    const MyliteAstUpdateAssignment *assignment);
size_t mylite_ast_update_assignment_view_name_value_length(
    const MyliteAstUpdateAssignment *assignment);
const MyliteAstNode *mylite_ast_update_assignment_view_value_node(
    const MyliteAstUpdateAssignment *assignment);
size_t mylite_ast_update_assignment_view_value_start(
    const MyliteAstUpdateAssignment *assignment);
size_t mylite_ast_update_assignment_view_value_end(
    const MyliteAstUpdateAssignment *assignment);
const MyliteAstExpression *
mylite_ast_update_assignment_view_value_expression(
    const MyliteAstUpdateAssignment *assignment);
const MyliteAstNode *mylite_ast_select_statement_view_node(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_end(
    const MyliteAstSelectStatement *select_statement);
int mylite_ast_select_statement_view_has_with_clause(
    const MyliteAstSelectStatement *select_statement);
int mylite_ast_select_statement_view_has_set_operation(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_query_block_count(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_projection_count(
    const MyliteAstSelectStatement *select_statement);
const MyliteAstSelectProjection *
mylite_ast_select_statement_view_projection_at(
    const MyliteAstSelectStatement *select_statement, size_t projection_index);
size_t mylite_ast_select_statement_view_from_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_from_end(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_where_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_where_end(
    const MyliteAstSelectStatement *select_statement);
const MyliteAstExpression *mylite_ast_select_statement_view_where_expression(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_group_by_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_group_by_end(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_having_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_having_end(
    const MyliteAstSelectStatement *select_statement);
const MyliteAstExpression *mylite_ast_select_statement_view_having_expression(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_order_by_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_order_by_end(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_limit_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_limit_end(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_into_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_into_end(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_lock_start(
    const MyliteAstSelectStatement *select_statement);
size_t mylite_ast_select_statement_view_lock_end(
    const MyliteAstSelectStatement *select_statement);
const MyliteAstNode *mylite_ast_select_projection_view_node(
    const MyliteAstSelectProjection *projection);
MyliteSelectProjectionKind mylite_ast_select_projection_view_kind(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_start(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_end(
    const MyliteAstSelectProjection *projection);
const MyliteAstNode *mylite_ast_select_projection_view_expression_node(
    const MyliteAstSelectProjection *projection);
const MyliteAstExpression *mylite_ast_select_projection_view_expression(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_expression_start(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_expression_end(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_alias_start(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_alias_end(
    const MyliteAstSelectProjection *projection);
const char *mylite_ast_select_projection_view_alias_value(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_alias_value_length(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_qualifier_start(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_qualifier_end(
    const MyliteAstSelectProjection *projection);
const char *mylite_ast_select_projection_view_qualifier_value(
    const MyliteAstSelectProjection *projection);
size_t mylite_ast_select_projection_view_qualifier_value_length(
    const MyliteAstSelectProjection *projection);
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
const MyliteAstNode *mylite_ast_create_database_view_node(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_start(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_end(
    const MyliteAstCreateDatabase *create_database);
int mylite_ast_create_database_view_has_if_not_exists(
    const MyliteAstCreateDatabase *create_database);
int mylite_ast_create_database_view_uses_schema_keyword(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_name_start(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_name_end(
    const MyliteAstCreateDatabase *create_database);
const char *mylite_ast_create_database_view_name_value(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_name_value_length(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_option_count(
    const MyliteAstCreateDatabase *create_database);
const MyliteAstDatabaseOption *mylite_ast_create_database_view_option_at(
    const MyliteAstCreateDatabase *create_database, size_t option_index);
const char *mylite_ast_create_database_view_charset_value(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_charset_value_length(
    const MyliteAstCreateDatabase *create_database);
const char *mylite_ast_create_database_view_collation_value(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_collation_value_length(
    const MyliteAstCreateDatabase *create_database);
const char *mylite_ast_create_database_view_encryption_value(
    const MyliteAstCreateDatabase *create_database);
size_t mylite_ast_create_database_view_encryption_value_length(
    const MyliteAstCreateDatabase *create_database);
const MyliteAstNode *mylite_ast_drop_database_view_node(
    const MyliteAstDropDatabase *drop_database);
size_t mylite_ast_drop_database_view_start(
    const MyliteAstDropDatabase *drop_database);
size_t mylite_ast_drop_database_view_end(
    const MyliteAstDropDatabase *drop_database);
int mylite_ast_drop_database_view_has_if_exists(
    const MyliteAstDropDatabase *drop_database);
int mylite_ast_drop_database_view_uses_schema_keyword(
    const MyliteAstDropDatabase *drop_database);
size_t mylite_ast_drop_database_view_name_start(
    const MyliteAstDropDatabase *drop_database);
size_t mylite_ast_drop_database_view_name_end(
    const MyliteAstDropDatabase *drop_database);
const char *mylite_ast_drop_database_view_name_value(
    const MyliteAstDropDatabase *drop_database);
size_t mylite_ast_drop_database_view_name_value_length(
    const MyliteAstDropDatabase *drop_database);
const MyliteAstNode *mylite_ast_database_option_view_node(
    const MyliteAstDatabaseOption *option);
MyliteDatabaseOptionKind mylite_ast_database_option_view_kind(
    const MyliteAstDatabaseOption *option);
MyliteDatabaseOptionValueKind mylite_ast_database_option_view_value_kind(
    const MyliteAstDatabaseOption *option);
size_t mylite_ast_database_option_view_start(
    const MyliteAstDatabaseOption *option);
size_t mylite_ast_database_option_view_end(
    const MyliteAstDatabaseOption *option);
size_t mylite_ast_database_option_view_name_start(
    const MyliteAstDatabaseOption *option);
size_t mylite_ast_database_option_view_name_end(
    const MyliteAstDatabaseOption *option);
size_t mylite_ast_database_option_view_value_start(
    const MyliteAstDatabaseOption *option);
size_t mylite_ast_database_option_view_value_end(
    const MyliteAstDatabaseOption *option);
const char *mylite_ast_database_option_view_value(
    const MyliteAstDatabaseOption *option);
size_t mylite_ast_database_option_view_value_length(
    const MyliteAstDatabaseOption *option);
const MyliteAstNode *mylite_ast_create_view_view_node(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_start(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_end(
    const MyliteAstCreateView *create_view);
int mylite_ast_create_view_view_has_or_replace(
    const MyliteAstCreateView *create_view);
MyliteCreateViewAlgorithm mylite_ast_create_view_view_algorithm(
    const MyliteAstCreateView *create_view);
MyliteViewSqlSecurity mylite_ast_create_view_view_sql_security(
    const MyliteAstCreateView *create_view);
MyliteViewCheckOption mylite_ast_create_view_view_check_option(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_name_start(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_name_end(
    const MyliteAstCreateView *create_view);
const char *mylite_ast_create_view_view_schema_value(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_schema_value_length(
    const MyliteAstCreateView *create_view);
const char *mylite_ast_create_view_view_name_value(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_name_value_length(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_definer_start(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_definer_end(
    const MyliteAstCreateView *create_view);
const MyliteAstNode *mylite_ast_create_view_view_select_node(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_select_start(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_select_end(
    const MyliteAstCreateView *create_view);
size_t mylite_ast_create_view_view_column_count(
    const MyliteAstCreateView *create_view);
const MyliteAstViewColumn *mylite_ast_create_view_view_column_at(
    const MyliteAstCreateView *create_view, size_t column_index);
size_t mylite_ast_view_column_view_start(const MyliteAstViewColumn *column);
size_t mylite_ast_view_column_view_end(const MyliteAstViewColumn *column);
const char *mylite_ast_view_column_view_name_value(
    const MyliteAstViewColumn *column);
size_t mylite_ast_view_column_view_name_value_length(
    const MyliteAstViewColumn *column);
const MyliteAstNode *mylite_ast_drop_view_view_node(
    const MyliteAstDropView *drop_view);
size_t mylite_ast_drop_view_view_start(const MyliteAstDropView *drop_view);
size_t mylite_ast_drop_view_view_end(const MyliteAstDropView *drop_view);
int mylite_ast_drop_view_view_has_if_exists(
    const MyliteAstDropView *drop_view);
MyliteDropViewMode mylite_ast_drop_view_view_mode(
    const MyliteAstDropView *drop_view);
size_t mylite_ast_drop_view_view_view_count(
    const MyliteAstDropView *drop_view);
const char *mylite_ast_drop_view_view_schema_value_at(
    const MyliteAstDropView *drop_view, size_t view_index);
size_t mylite_ast_drop_view_view_schema_value_length_at(
    const MyliteAstDropView *drop_view, size_t view_index);
const char *mylite_ast_drop_view_view_name_value_at(
    const MyliteAstDropView *drop_view, size_t view_index);
size_t mylite_ast_drop_view_view_name_value_length_at(
    const MyliteAstDropView *drop_view, size_t view_index);
const MyliteAstNode *mylite_ast_set_statement_view_node(
    const MyliteAstSetStatement *set_statement);
size_t mylite_ast_set_statement_view_start(
    const MyliteAstSetStatement *set_statement);
size_t mylite_ast_set_statement_view_end(
    const MyliteAstSetStatement *set_statement);
MyliteSetStatementForm mylite_ast_set_statement_view_form(
    const MyliteAstSetStatement *set_statement);
size_t mylite_ast_set_statement_view_assignment_count(
    const MyliteAstSetStatement *set_statement);
const MyliteAstSetAssignment *mylite_ast_set_statement_view_assignment_at(
    const MyliteAstSetStatement *set_statement, size_t assignment_index);
const MyliteAstNode *mylite_ast_set_assignment_view_node(
    const MyliteAstSetAssignment *assignment);
MyliteSetAssignmentKind mylite_ast_set_assignment_view_kind(
    const MyliteAstSetAssignment *assignment);
MyliteSetVariableScope mylite_ast_set_assignment_view_scope(
    const MyliteAstSetAssignment *assignment);
MyliteSetAssignmentOperator mylite_ast_set_assignment_view_operator(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_start(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_end(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_name_start(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_name_end(
    const MyliteAstSetAssignment *assignment);
const char *mylite_ast_set_assignment_view_name_value(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_name_value_length(
    const MyliteAstSetAssignment *assignment);
const MyliteAstNode *mylite_ast_set_assignment_view_value_node(
    const MyliteAstSetAssignment *assignment);
const MyliteAstExpression *mylite_ast_set_assignment_view_value_expression(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_value_start(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_value_end(
    const MyliteAstSetAssignment *assignment);
const MyliteAstNode *mylite_ast_set_assignment_view_extend_value_node(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_extend_value_start(
    const MyliteAstSetAssignment *assignment);
size_t mylite_ast_set_assignment_view_extend_value_end(
    const MyliteAstSetAssignment *assignment);
const MyliteAstNode *mylite_ast_prepare_statement_view_node(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_start(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_end(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_name_start(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_name_end(
    const MyliteAstPrepareStatement *prepare_statement);
const char *mylite_ast_prepare_statement_view_name_value(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_name_value_length(
    const MyliteAstPrepareStatement *prepare_statement);
MylitePrepareStatementSourceKind
mylite_ast_prepare_statement_view_source_kind(
    const MyliteAstPrepareStatement *prepare_statement);
const MyliteAstNode *mylite_ast_prepare_statement_view_source_node(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_source_start(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_source_end(
    const MyliteAstPrepareStatement *prepare_statement);
const char *mylite_ast_prepare_statement_view_source_value(
    const MyliteAstPrepareStatement *prepare_statement);
size_t mylite_ast_prepare_statement_view_source_value_length(
    const MyliteAstPrepareStatement *prepare_statement);
const MyliteAstNode *mylite_ast_execute_statement_view_node(
    const MyliteAstExecuteStatement *execute_statement);
size_t mylite_ast_execute_statement_view_start(
    const MyliteAstExecuteStatement *execute_statement);
size_t mylite_ast_execute_statement_view_end(
    const MyliteAstExecuteStatement *execute_statement);
size_t mylite_ast_execute_statement_view_name_start(
    const MyliteAstExecuteStatement *execute_statement);
size_t mylite_ast_execute_statement_view_name_end(
    const MyliteAstExecuteStatement *execute_statement);
const char *mylite_ast_execute_statement_view_name_value(
    const MyliteAstExecuteStatement *execute_statement);
size_t mylite_ast_execute_statement_view_name_value_length(
    const MyliteAstExecuteStatement *execute_statement);
size_t mylite_ast_execute_statement_view_using_count(
    const MyliteAstExecuteStatement *execute_statement);
const MyliteAstPreparedStatementVariable *
mylite_ast_execute_statement_view_using_variable_at(
    const MyliteAstExecuteStatement *execute_statement, size_t variable_index);
const MyliteAstNode *mylite_ast_prepared_statement_variable_view_node(
    const MyliteAstPreparedStatementVariable *variable);
size_t mylite_ast_prepared_statement_variable_view_start(
    const MyliteAstPreparedStatementVariable *variable);
size_t mylite_ast_prepared_statement_variable_view_end(
    const MyliteAstPreparedStatementVariable *variable);
size_t mylite_ast_prepared_statement_variable_view_name_start(
    const MyliteAstPreparedStatementVariable *variable);
size_t mylite_ast_prepared_statement_variable_view_name_end(
    const MyliteAstPreparedStatementVariable *variable);
const char *mylite_ast_prepared_statement_variable_view_name_value(
    const MyliteAstPreparedStatementVariable *variable);
size_t mylite_ast_prepared_statement_variable_view_name_value_length(
    const MyliteAstPreparedStatementVariable *variable);
const MyliteAstNode *mylite_ast_deallocate_statement_view_node(
    const MyliteAstDeallocateStatement *deallocate_statement);
size_t mylite_ast_deallocate_statement_view_start(
    const MyliteAstDeallocateStatement *deallocate_statement);
size_t mylite_ast_deallocate_statement_view_end(
    const MyliteAstDeallocateStatement *deallocate_statement);
MyliteDeallocateStatementMode mylite_ast_deallocate_statement_view_mode(
    const MyliteAstDeallocateStatement *deallocate_statement);
size_t mylite_ast_deallocate_statement_view_name_start(
    const MyliteAstDeallocateStatement *deallocate_statement);
size_t mylite_ast_deallocate_statement_view_name_end(
    const MyliteAstDeallocateStatement *deallocate_statement);
const char *mylite_ast_deallocate_statement_view_name_value(
    const MyliteAstDeallocateStatement *deallocate_statement);
size_t mylite_ast_deallocate_statement_view_name_value_length(
    const MyliteAstDeallocateStatement *deallocate_statement);
const MyliteAstNode *mylite_ast_transaction_statement_view_node(
    const MyliteAstTransactionStatement *transaction_statement);
size_t mylite_ast_transaction_statement_view_start(
    const MyliteAstTransactionStatement *transaction_statement);
size_t mylite_ast_transaction_statement_view_end(
    const MyliteAstTransactionStatement *transaction_statement);
MyliteTransactionStatementKind mylite_ast_transaction_statement_view_kind(
    const MyliteAstTransactionStatement *transaction_statement);
MyliteTransactionBeginForm mylite_ast_transaction_statement_view_begin_form(
    const MyliteAstTransactionStatement *transaction_statement);
MyliteTransactionBeginMode mylite_ast_transaction_statement_view_begin_mode(
    const MyliteAstTransactionStatement *transaction_statement);
MyliteTransactionAccessMode mylite_ast_transaction_statement_view_access_mode(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_consistent_snapshot(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_causal_consistency(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_work_keyword(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_chain(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_no_chain(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_release(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_no_release(
    const MyliteAstTransactionStatement *transaction_statement);
int mylite_ast_transaction_statement_view_has_savepoint_keyword(
    const MyliteAstTransactionStatement *transaction_statement);
size_t mylite_ast_transaction_statement_view_savepoint_name_start(
    const MyliteAstTransactionStatement *transaction_statement);
size_t mylite_ast_transaction_statement_view_savepoint_name_end(
    const MyliteAstTransactionStatement *transaction_statement);
const char *mylite_ast_transaction_statement_view_savepoint_name_value(
    const MyliteAstTransactionStatement *transaction_statement);
size_t mylite_ast_transaction_statement_view_savepoint_name_value_length(
    const MyliteAstTransactionStatement *transaction_statement);
const MyliteAstNode *mylite_ast_expression_view_node(
    const MyliteAstExpression *expression);
MyliteExpressionKind mylite_ast_expression_view_kind(
    const MyliteAstExpression *expression);
MyliteExpressionLiteralKind mylite_ast_expression_view_literal_kind(
    const MyliteAstExpression *expression);
MyliteExpressionOperatorKind mylite_ast_expression_view_operator_kind(
    const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_start(const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_end(const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_operator_start(
    const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_operator_end(
    const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_value_start(
    const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_value_end(
    const MyliteAstExpression *expression);
const char *mylite_ast_expression_view_value(
    const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_value_length(
    const MyliteAstExpression *expression);
int mylite_ast_expression_view_has_unsigned_integer(
    const MyliteAstExpression *expression);
unsigned long long mylite_ast_expression_view_unsigned_integer_value(
    const MyliteAstExpression *expression);
size_t mylite_ast_expression_view_child_count(
    const MyliteAstExpression *expression);
const MyliteAstExpression *mylite_ast_expression_view_child_at(
    const MyliteAstExpression *expression, size_t index);
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
const MyliteAstNode *mylite_ast_use_database_view_node(
    const MyliteAstUseDatabase *use_database);
size_t mylite_ast_use_database_view_start(
    const MyliteAstUseDatabase *use_database);
size_t mylite_ast_use_database_view_end(
    const MyliteAstUseDatabase *use_database);
size_t mylite_ast_use_database_view_name_start(
    const MyliteAstUseDatabase *use_database);
size_t mylite_ast_use_database_view_name_end(
    const MyliteAstUseDatabase *use_database);
const char *mylite_ast_use_database_view_name_value(
    const MyliteAstUseDatabase *use_database);
size_t mylite_ast_use_database_view_name_value_length(
    const MyliteAstUseDatabase *use_database);
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
const MyliteAstExpression *
mylite_ast_create_table_column_view_default_value_expression(
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
const MyliteAstExpression *
mylite_ast_create_table_column_view_on_update_value_expression(
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
const MyliteAstExpression *
mylite_ast_create_table_column_view_generated_expression(
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
const MyliteAstExpression *
mylite_ast_create_table_column_view_check_expression(
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
const MyliteAstNode *mylite_ast_create_table_key_view_check_expression_node(
    const MyliteAstCreateTableKey *key);
const MyliteAstExpression *mylite_ast_create_table_key_view_check_expression(
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
const MyliteAstNode *mylite_ast_create_table_key_part_view_expression_node(
    const MyliteAstCreateTableKeyPart *part);
const MyliteAstExpression *mylite_ast_create_table_key_part_view_expression(
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
