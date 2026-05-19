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
    MYLITE_SQL_AST_ROW_COUNT_FUNCTION = 47,
    MYLITE_SQL_AST_SESSION_USER_FUNCTION = 48,
    MYLITE_SQL_AST_SYSTEM_USER_FUNCTION = 49,
    MYLITE_SQL_AST_CONNECTION_ID_FUNCTION = 50,
    MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR = 51,
    MYLITE_SQL_AST_COUNT_STAR_FUNCTION = 52,
    MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT = 53,
    MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT = 54,
    MYLITE_SQL_AST_TABLE_ENGINE_OPTION = 55,
    MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT = 56,
    MYLITE_SQL_AST_TABLE_OPTION_LIST = 57,
    MYLITE_SQL_AST_TABLE_CHARSET_OPTION = 58,
    MYLITE_SQL_AST_TABLE_COLLATION_OPTION = 59,
    MYLITE_SQL_AST_INSERT_SET_STATEMENT = 60,
    MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST = 61,
    MYLITE_SQL_AST_INSERT_ASSIGNMENT = 62,
    MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT = 63,
    MYLITE_SQL_AST_SHOW_INDEX_STATEMENT = 64,
    MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT = 65,
    MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT = 66,
    MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT = 67,
    MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT = 68,
    MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT = 69,
    MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT = 70,
    MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT = 71,
    MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT = 72,
    MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT = 73,
    MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT = 74,
    MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT = 75,
    MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT = 76,
    MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT = 77,
    MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT = 78,
    MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT = 79,
    MYLITE_SQL_AST_SYSTEM_VARIABLE = 80,
    MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION = 81,
    MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR = 82,
    MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT = 83,
    MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT = 84,
    MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT = 85,
    MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT = 86,
    MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT = 87,
    MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE = 88,
    MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE = 89,
    MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE = 90,
    MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE = 91,
    MYLITE_SQL_AST_TABLE_NAME_LIST = 92,
    MYLITE_SQL_AST_RENAME_TABLE_PAIR = 93,
    MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST = 94,
    MYLITE_SQL_AST_COLUMN_DEFAULT_NULL = 95,
    MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE = 96,
    MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT = 97,
    MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT = 98,
    MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT = 99,
    MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION = 100,
    MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION = 101,
    MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION = 102,
    MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION = 103,
    MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION = 104,
    MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION = 105,
    MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT = 106,
    MYLITE_SQL_AST_REPLACE_SET_STATEMENT = 107,
    MYLITE_SQL_AST_SET_NAMES_STATEMENT = 108,
    MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT = 109,
    MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET = 110,
    MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT = 111,
    MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT = 112,
    MYLITE_SQL_AST_ORDER_BY_ITEM_LIST = 113,
    MYLITE_SQL_AST_ORDER_BY_ITEM = 114,
    MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT = 115,
    MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT = 116,
    MYLITE_SQL_AST_INSERT_SELECT_STATEMENT = 117,
    MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT = 118,
    MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT = 119,
    MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER = 120,
    MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER = 121,
    MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER = 122,
    MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER = 123,
    MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER = 124,
    MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER = 125,
    MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION = 126,
    MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION = 127,
    MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION = 128,
    MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION = 129,
    MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION = 130,
    MYLITE_SQL_AST_GROUP_BY_CLAUSE = 131,
    MYLITE_SQL_AST_HAVING_CLAUSE = 132,
    MYLITE_SQL_AST_AND_PREDICATE = 133,
    MYLITE_SQL_AST_OR_PREDICATE = 134,
    MYLITE_SQL_AST_NOT_PREDICATE = 135,
    MYLITE_SQL_AST_BETWEEN_PREDICATE = 136,
    MYLITE_SQL_AST_IN_PREDICATE = 137,
    MYLITE_SQL_AST_PREDICATE_VALUE_LIST = 138,
    MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE = 139,
    MYLITE_SQL_AST_XOR_PREDICATE = 140,
    MYLITE_SQL_AST_DML_DEFAULT_VALUE = 141,
    MYLITE_SQL_AST_IF_FUNCTION = 142,
    MYLITE_SQL_AST_IFNULL_FUNCTION = 143,
    MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR = 144,
    MYLITE_SQL_AST_COALESCE_FUNCTION = 145,
    MYLITE_SQL_AST_NULLIF_FUNCTION = 146,
    MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR = 147,
    MYLITE_SQL_AST_ISNULL_FUNCTION = 148,
    MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR = 149,
    MYLITE_SQL_AST_MOD_FUNCTION = 150,
    MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION = 151,
    MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION = 152,
    MYLITE_SQL_AST_CASE_WHEN_LIST = 153,
    MYLITE_SQL_AST_CASE_WHEN_CLAUSE = 154,
    MYLITE_SQL_AST_CASE_ELSE_CLAUSE = 155,
    MYLITE_SQL_AST_DO_EXPRESSION_LIST = 156,
    MYLITE_SQL_AST_DO_STATEMENT = 157,
    MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT = 158,
    MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET = 159,
    MYLITE_SQL_AST_SET_DEFAULT_VALUE = 160,
    MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT = 161,
    MYLITE_SQL_AST_FOUND_ROWS_FUNCTION = 162,
    MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR = 163,
    MYLITE_SQL_AST_BIT_COUNT_FUNCTION = 164,
    MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR = 165,
    MYLITE_SQL_AST_ABS_FUNCTION = 166,
    MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR = 167,
    MYLITE_SQL_AST_SIGN_FUNCTION = 168,
    MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR = 169,
    MYLITE_SQL_AST_CEIL_FUNCTION = 170,
    MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR = 171,
    MYLITE_SQL_AST_CEILING_FUNCTION = 172,
    MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR = 173,
    MYLITE_SQL_AST_FLOOR_FUNCTION = 174,
    MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR = 175,
    MYLITE_SQL_AST_ROUND_FUNCTION = 176,
    MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR = 177,
    MYLITE_SQL_AST_BIN_FUNCTION = 178,
    MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR = 179,
    MYLITE_SQL_AST_OCT_FUNCTION = 180,
    MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR = 181,
    MYLITE_SQL_AST_CONV_FUNCTION = 182,
    MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR = 183,
    MYLITE_SQL_AST_PI_FUNCTION = 184,
    MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR = 185,
    MYLITE_SQL_AST_SQRT_FUNCTION = 186,
    MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR = 187,
    MYLITE_SQL_AST_DEGREES_FUNCTION = 188,
    MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR = 189,
    MYLITE_SQL_AST_RADIANS_FUNCTION = 190,
    MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR = 191,
    MYLITE_SQL_AST_ACOS_FUNCTION = 192,
    MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR = 193,
    MYLITE_SQL_AST_ASIN_FUNCTION = 194,
    MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR = 195,
    MYLITE_SQL_AST_ATAN_FUNCTION = 196,
    MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR = 197,
    MYLITE_SQL_AST_ATAN2_FUNCTION = 198,
    MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR = 199,
    MYLITE_SQL_AST_VARCHAR_TYPE = 200,
    MYLITE_SQL_AST_CHAR_TYPE = 201,
    MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION = 202,
    MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST = 203,
    MYLITE_SQL_AST_INLINE_PRIMARY_KEY = 204,
    MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST = 205,
    MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT = 206,
    MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION = 207,
    MYLITE_SQL_AST_TEXT_TYPE = 208,
    MYLITE_SQL_AST_DECIMAL_TYPE = 209,
    MYLITE_SQL_AST_DATE_TYPE = 210,
    MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION = 211,
    MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST = 212,
    MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION = 213,
    MYLITE_SQL_AST_INLINE_UNIQUE_KEY = 214,
    MYLITE_SQL_AST_DATETIME_TYPE = 215,
    MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT = 216,
    MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT = 217,
    MYLITE_SQL_AST_TIMESTAMP_TYPE = 218,
    MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT = 219,
    MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT = 220,
    MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT = 221,
    MYLITE_SQL_AST_CREATE_INDEX_STATEMENT = 222,
    MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT = 223,
    MYLITE_SQL_AST_DROP_INDEX_STATEMENT = 224,
    MYLITE_SQL_AST_TIME_TYPE = 225,
    MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE = 226,
    MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST = 227,
    MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT = 228,
    MYLITE_SQL_AST_INSERT_VALUES_REFERENCE = 229,
    MYLITE_SQL_AST_APPROXIMATE_TYPE = 230,
    MYLITE_SQL_AST_SECONDARY_INDEX_PART = 231,
    MYLITE_SQL_AST_CONCAT_FUNCTION = 232,
    MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR = 233,
    MYLITE_SQL_AST_SCALAR_SUBQUERY = 234,
    MYLITE_SQL_AST_CAST_BINARY_EXPRESSION = 235,
    MYLITE_SQL_AST_DATE_ADD_FUNCTION = 236,
    MYLITE_SQL_AST_FIELD_FUNCTION = 237,
    MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR = 238,
    MYLITE_SQL_AST_RAND_FUNCTION = 239,
    MYLITE_SQL_AST_RAND_SEED_FUNCTION = 240,
    MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR = 241,
    MYLITE_SQL_AST_DATE_FORMAT_FUNCTION = 242,
    MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR = 243,
    MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE = 244,
    MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR = 245,
    MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP = 246,
    MYLITE_SQL_AST_BINARY_STRING_TYPE = 247,
    MYLITE_SQL_AST_BIT_TYPE = 248,
    MYLITE_SQL_AST_YEAR_TYPE = 249,
    MYLITE_SQL_AST_START_TRANSACTION_STATEMENT = 250,
    MYLITE_SQL_AST_COMMIT_STATEMENT = 251,
    MYLITE_SQL_AST_ROLLBACK_STATEMENT = 252,
    MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT = 253,
    MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT = 254,
    MYLITE_SQL_AST_FROM_JOIN = 255,
    MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION = 256,
    MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST = 257,
    MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT = 258,
    MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT = 259,
    MYLITE_SQL_AST_LENGTH_FUNCTION = 260,
    MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR = 261,
    MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION = 262,
    MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR = 263,
    MYLITE_SQL_AST_BIT_LENGTH_FUNCTION = 264,
    MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR = 265,
    MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION = 266,
    MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR = 267,
    MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION = 268,
    MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR = 269,
    MYLITE_SQL_AST_ENUM_TYPE = 270,
    MYLITE_SQL_AST_ENUM_LABEL_LIST = 271,
    MYLITE_SQL_AST_SET_TYPE = 272,
    MYLITE_SQL_AST_SET_MEMBER_LIST = 273,
    MYLITE_SQL_AST_EXP_FUNCTION = 274,
    MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR = 275,
    MYLITE_SQL_AST_LN_FUNCTION = 276,
    MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR = 277,
    MYLITE_SQL_AST_LOG_FUNCTION = 278,
    MYLITE_SQL_AST_LOG10_FUNCTION = 279,
    MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR = 280,
    MYLITE_SQL_AST_LOG2_FUNCTION = 281,
    MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR = 282,
    MYLITE_SQL_AST_POW_FUNCTION = 283,
    MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR = 284,
    MYLITE_SQL_AST_POWER_FUNCTION = 285,
    MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR = 286,
    MYLITE_SQL_AST_JSON_TYPE = 287,
    MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT = 288,
    MYLITE_SQL_AST_SAVEPOINT_STATEMENT = 289,
    MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT = 290,
    MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT = 291,
    MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT = 292,
    MYLITE_SQL_AST_CHECK_TABLE_STATEMENT = 293,
    MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT = 294,
    MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT = 295,
    MYLITE_SQL_AST_LOWER_FUNCTION = 296,
    MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR = 297,
    MYLITE_SQL_AST_LCASE_FUNCTION = 298,
    MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR = 299,
    MYLITE_SQL_AST_UPPER_FUNCTION = 300,
    MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR = 301,
    MYLITE_SQL_AST_UCASE_FUNCTION = 302,
    MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR = 303,
    MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION = 304,
    MYLITE_SQL_AST_LOCK_TABLES_STATEMENT = 305,
    MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT = 306,
    MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST = 307,
    MYLITE_SQL_AST_LOCK_TABLE_TARGET = 308,
    MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK = 309,
    MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK = 310,
    MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK = 311,
    MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT = 312,
    MYLITE_SQL_AST_CURRENT_DATE_VALUE = 313,
    MYLITE_SQL_AST_CURRENT_TIME_VALUE = 314,
    MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE = 315,
    MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE = 316,
    MYLITE_SQL_AST_EXISTS_PREDICATE = 317,
    MYLITE_SQL_AST_SIN_FUNCTION = 318,
    MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR = 319,
    MYLITE_SQL_AST_COS_FUNCTION = 320,
    MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR = 321,
    MYLITE_SQL_AST_TAN_FUNCTION = 322,
    MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR = 323,
    MYLITE_SQL_AST_COT_FUNCTION = 324,
    MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR = 325,
    MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT = 326,
    MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST = 327,
    MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ = 328,
    MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED = 329,
    MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_UNCOMMITTED = 330,
    MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE = 331,
    MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE = 332,
    MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY = 333,
    MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT = 334,
    MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT = 335,
    MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION = 336,
    MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED = 337,
    MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED = 338,
    MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT = 339,
    MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT = 340,
    MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT = 341,
    MYLITE_SQL_AST_CRC32_FUNCTION = 342,
    MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR = 343,
    MYLITE_SQL_AST_FORMAT_FUNCTION = 344,
    MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED = 345,
    MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR = 346,
    MYLITE_SQL_AST_TRUNCATE_FUNCTION = 347,
    MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR = 348,
    MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME = 349,
    MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST = 350,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE = 351,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_RESTRICT = 352,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION = 353,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE = 354,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT = 355,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_NO_ACTION = 356,
    MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION = 357,
    MYLITE_SQL_AST_LEFT_FUNCTION = 358,
    MYLITE_SQL_AST_RIGHT_FUNCTION = 359,
    MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION = 360,
    MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION = 361,
    MYLITE_SQL_AST_COLUMN_POSITION_FIRST = 362,
    MYLITE_SQL_AST_COLUMN_POSITION_AFTER = 363,
    MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT = 364,
    MYLITE_SQL_AST_UNION_TERM_LIST = 365,
    MYLITE_SQL_AST_UNION_TERM = 366,
    MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION = 367,
    MYLITE_SQL_AST_HEX_FUNCTION = 368,
    MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR = 369,
    MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT = 370,
    MYLITE_SQL_AST_SUBSTRING_FUNCTION = 371,
    MYLITE_SQL_AST_SUBSTR_FUNCTION = 372,
    MYLITE_SQL_AST_MID_FUNCTION = 373,
    MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT = 374,
    MYLITE_SQL_AST_CHARSET_FUNCTION = 375,
    MYLITE_SQL_AST_COLLATION_FUNCTION = 376,
    MYLITE_SQL_AST_LTRIM_FUNCTION = 377,
    MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR = 378,
    MYLITE_SQL_AST_RTRIM_FUNCTION = 379,
    MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR = 380,
    MYLITE_SQL_AST_TRIM_FUNCTION = 381,
    MYLITE_SQL_AST_TRIM_LEADING_FUNCTION = 382,
    MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION = 383,
    MYLITE_SQL_AST_DATE_FUNCTION = 384,
    MYLITE_SQL_AST_YEAR_FUNCTION = 385,
    MYLITE_SQL_AST_MONTH_FUNCTION = 386,
    MYLITE_SQL_AST_DAY_FUNCTION = 387,
    MYLITE_SQL_AST_DAYOFMONTH_FUNCTION = 388,
    MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR = 389,
    MYLITE_SQL_AST_HOUR_FUNCTION = 390,
    MYLITE_SQL_AST_MINUTE_FUNCTION = 391,
    MYLITE_SQL_AST_SECOND_FUNCTION = 392,
    MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT = 393,
    MYLITE_SQL_AST_LOCATE_FUNCTION = 394,
    MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR = 395,
    MYLITE_SQL_AST_INSTR_FUNCTION = 396,
    MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR = 397,
    MYLITE_SQL_AST_POSITION_FUNCTION = 398,
    MYLITE_SQL_AST_FIND_IN_SET_FUNCTION = 399,
    MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR = 400,
    MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT = 401,
    MYLITE_SQL_AST_DATE_SUB_FUNCTION = 402,
    MYLITE_SQL_AST_ADDDATE_FUNCTION = 403,
    MYLITE_SQL_AST_SUBDATE_FUNCTION = 404,
    MYLITE_SQL_AST_CONCAT_WS_FUNCTION = 405,
    MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR = 406,
    MYLITE_SQL_AST_JSON_VALID_FUNCTION = 407,
    MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR = 408,
    MYLITE_SQL_AST_CAST_CHAR_EXPRESSION = 409,
    MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION = 410,
    MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION = 411,
    MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION = 412,
    MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION = 413,
    MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION = 414,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL = 415,
    MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL = 416,
    MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION = 417,
    MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR = 418,
    MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION = 419,
    MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR = 420,
    MYLITE_SQL_AST_JSON_ARRAY_FUNCTION = 421,
    MYLITE_SQL_AST_JSON_OBJECT_FUNCTION = 422,
    MYLITE_SQL_AST_SHOW_STATUS_STATEMENT = 423,
    MYLITE_SQL_AST_JSON_TYPE_FUNCTION = 424,
    MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR = 425,
    MYLITE_SQL_AST_JSON_LENGTH_FUNCTION = 426,
    MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR = 427,
    MYLITE_SQL_AST_INDEX_HINT_LIST = 428,
    MYLITE_SQL_AST_USE_INDEX_HINT = 429,
    MYLITE_SQL_AST_FORCE_INDEX_HINT = 430,
    MYLITE_SQL_AST_IGNORE_INDEX_HINT = 431,
    MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN = 432,
    MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY = 433,
    MYLITE_SQL_AST_INDEX_HINT_FOR_GROUP_BY = 434,
    MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT = 435,
    MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION = 436,
    MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR = 437,
    MYLITE_SQL_AST_REPLACE_FUNCTION = 438,
    MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION = 439,
    MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR = 440,
    MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION = 441,
    MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR = 442,
    MYLITE_SQL_AST_UNHEX_FUNCTION = 443,
    MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR = 444,
    MYLITE_SQL_AST_DEFAULT_FUNCTION = 445,
    MYLITE_SQL_AST_ASCII_FUNCTION = 446,
    MYLITE_SQL_AST_ORD_FUNCTION = 447,
    MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR = 448,
    MYLITE_SQL_AST_JOINED_DELETE_STATEMENT = 449,
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
    MYLITE_SQL_AST_OPERATOR_LOGICAL_AND = 16,
    MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND = 17,
    MYLITE_SQL_AST_OPERATOR_LOGICAL_OR = 18,
    MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR = 19,
    MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT = 20,
    MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR = 21,
    MYLITE_SQL_AST_OPERATOR_IS_TRUE = 22,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE = 23,
    MYLITE_SQL_AST_OPERATOR_IS_FALSE = 24,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE = 25,
    MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN = 26,
    MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN = 27,
    MYLITE_SQL_AST_OPERATOR_MODULO = 28,
    MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE = 29,
    MYLITE_SQL_AST_OPERATOR_BITWISE_NOT = 30,
    MYLITE_SQL_AST_OPERATOR_BITWISE_XOR = 31,
    MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT = 32,
    MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT = 33,
    MYLITE_SQL_AST_OPERATOR_BITWISE_AND = 34,
    MYLITE_SQL_AST_OPERATOR_BITWISE_OR = 35,
    MYLITE_SQL_AST_OPERATOR_LIKE = 36,
    MYLITE_SQL_AST_OPERATOR_REGEXP = 37,
    MYLITE_SQL_AST_OPERATOR_RLIKE = 38,
    MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT = 39,
    MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT = 40,
};

enum mylite_sql_ast_integer_type {
    MYLITE_SQL_AST_INTEGER_TYPE_NONE = 0,
    MYLITE_SQL_AST_INTEGER_TYPE_INT = 1,
    MYLITE_SQL_AST_INTEGER_TYPE_BIGINT = 2,
    MYLITE_SQL_AST_INTEGER_TYPE_TINYINT = 3,
    MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT = 4,
    MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT = 5,
};

enum mylite_sql_ast_text_type {
    MYLITE_SQL_AST_TEXT_TYPE_NONE = 0,
    MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT = 1,
    MYLITE_SQL_AST_TEXT_TYPE_TEXT = 2,
    MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT = 3,
    MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT = 4,
};

enum mylite_sql_ast_binary_string_type {
    MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE = 0,
    MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY = 1,
    MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY = 2,
    MYLITE_SQL_AST_BINARY_STRING_TYPE_TINYBLOB = 3,
    MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB = 4,
    MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB = 5,
    MYLITE_SQL_AST_BINARY_STRING_TYPE_LONGBLOB = 6,
};

enum mylite_sql_ast_decimal_type {
    MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL = 1,
    MYLITE_SQL_AST_DECIMAL_TYPE_DEC = 2,
    MYLITE_SQL_AST_DECIMAL_TYPE_NUMERIC = 3,
    MYLITE_SQL_AST_DECIMAL_TYPE_FIXED = 4,
};

enum mylite_sql_ast_approximate_type {
    MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT = 1,
    MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT4 = 2,
    MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT8 = 3,
    MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE = 4,
    MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL = 5,
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

enum mylite_sql_ast_column_visibility {
    MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE = 0,
    MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE = 1,
};

enum mylite_sql_ast_alter_algorithm {
    MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED = 0,
    MYLITE_SQL_AST_ALTER_ALGORITHM_DEFAULT = 1,
    MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT = 2,
    MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE = 3,
    MYLITE_SQL_AST_ALTER_ALGORITHM_COPY = 4,
    MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN = 5,
};

enum mylite_sql_ast_alter_lock {
    MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED = 0,
    MYLITE_SQL_AST_ALTER_LOCK_DEFAULT = 1,
    MYLITE_SQL_AST_ALTER_LOCK_NONE = 2,
    MYLITE_SQL_AST_ALTER_LOCK_SHARED = 3,
    MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE = 4,
    MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN = 5,
};

enum mylite_sql_ast_select_modifier {
    MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT = 0,
    MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT = 1,
};

enum mylite_sql_ast_union_modifier {
    MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT = 0,
    MYLITE_SQL_AST_UNION_MODIFIER_ALL = 1,
};

enum mylite_sql_ast_select_option {
    MYLITE_SQL_AST_SELECT_OPTION_HIGH_PRIORITY = 1U << 0U,
    MYLITE_SQL_AST_SELECT_OPTION_STRAIGHT_JOIN = 1U << 1U,
    MYLITE_SQL_AST_SELECT_OPTION_SQL_SMALL_RESULT = 1U << 2U,
    MYLITE_SQL_AST_SELECT_OPTION_SQL_BIG_RESULT = 1U << 3U,
    MYLITE_SQL_AST_SELECT_OPTION_SQL_BUFFER_RESULT = 1U << 4U,
    MYLITE_SQL_AST_SELECT_OPTION_SQL_NO_CACHE = 1U << 5U,
};

enum mylite_sql_ast_select_locking_clause {
    MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE = 0,
    MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE = 1,
    MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE = 2,
    MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_LOCK_IN_SHARE_MODE = 3,
};

enum mylite_sql_ast_join_kind {
    MYLITE_SQL_AST_JOIN_KIND_UNSPECIFIED = 0,
    MYLITE_SQL_AST_JOIN_KIND_INNER = 1,
    MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER = 2,
};

struct mylite_sql_ast_select_payload {
    enum mylite_sql_ast_select_modifier modifier;
    unsigned int options;
    int calc_found_rows;
    enum mylite_sql_ast_select_locking_clause locking_clause;
};

struct mylite_sql_ast_union_payload {
    enum mylite_sql_ast_union_modifier modifier;
};

struct mylite_sql_ast_join_payload {
    enum mylite_sql_ast_join_kind kind;
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
    int has_display_width;
    int is_bool_alias;
    int is_serial_alias;
    struct mylite_sql_source_span display_width_span;
};

struct mylite_sql_ast_varchar_type_payload {
    int is_national;
    struct mylite_sql_source_span length_span;
};

struct mylite_sql_ast_char_type_payload {
    int has_explicit_length;
    int is_national;
    struct mylite_sql_source_span length_span;
};

struct mylite_sql_ast_text_type_payload {
    enum mylite_sql_ast_text_type kind;
};

struct mylite_sql_ast_binary_string_type_payload {
    enum mylite_sql_ast_binary_string_type kind;
    int has_length;
    struct mylite_sql_source_span length_span;
};

struct mylite_sql_ast_bit_type_payload {
    int has_length;
    struct mylite_sql_source_span length_span;
};

struct mylite_sql_ast_year_type_payload {
    int has_width;
    struct mylite_sql_source_span width_span;
};

struct mylite_sql_ast_decimal_type_payload {
    enum mylite_sql_ast_decimal_type kind;
    int has_precision;
    int has_scale;
    int is_unsigned;
    struct mylite_sql_source_span precision_span;
    struct mylite_sql_source_span scale_span;
};

struct mylite_sql_ast_approximate_type_payload {
    enum mylite_sql_ast_approximate_type kind;
    int has_precision;
    int is_unsigned;
    struct mylite_sql_source_span precision_span;
};

struct mylite_sql_ast_nullability_payload {
    enum mylite_sql_ast_nullability kind;
};

struct mylite_sql_ast_order_direction_payload {
    enum mylite_sql_ast_order_direction kind;
};

struct mylite_sql_ast_column_visibility_payload {
    enum mylite_sql_ast_column_visibility kind;
};

struct mylite_sql_ast_alter_table_options_payload {
    enum mylite_sql_ast_alter_algorithm algorithm;
    enum mylite_sql_ast_alter_lock lock;
    enum mylite_sql_ast_column_visibility visibility;
};

struct mylite_sql_ast_show_tables_payload {
    int is_full;
};

union mylite_sql_ast_node_payload {
    struct mylite_sql_ast_select_payload select;
    struct mylite_sql_ast_union_payload union_term;
    struct mylite_sql_ast_join_payload join;
    struct mylite_sql_ast_literal_payload literal;
    struct mylite_sql_ast_expression_payload expression;
    struct mylite_sql_ast_integer_type_payload integer_type;
    struct mylite_sql_ast_varchar_type_payload varchar_type;
    struct mylite_sql_ast_char_type_payload char_type;
    struct mylite_sql_ast_text_type_payload text_type;
    struct mylite_sql_ast_binary_string_type_payload binary_string_type;
    struct mylite_sql_ast_bit_type_payload bit_type;
    struct mylite_sql_ast_year_type_payload year_type;
    struct mylite_sql_ast_decimal_type_payload decimal_type;
    struct mylite_sql_ast_approximate_type_payload approximate_type;
    struct mylite_sql_ast_nullability_payload nullability;
    struct mylite_sql_ast_order_direction_payload order_direction;
    struct mylite_sql_ast_column_visibility_payload column_visibility;
    struct mylite_sql_ast_alter_table_options_payload alter_table_options;
    struct mylite_sql_ast_show_tables_payload show_tables;
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
void mylite_sql_ast_node_set_select_modifier(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_select_modifier modifier
);
void mylite_sql_ast_node_set_select_options(struct mylite_sql_ast_node *node, unsigned int options);
void mylite_sql_ast_node_set_select_calc_found_rows(
    struct mylite_sql_ast_node *node,
    int calc_found_rows
);
void mylite_sql_ast_node_set_select_locking_clause(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_select_locking_clause locking_clause
);
void mylite_sql_ast_node_set_show_tables_full(struct mylite_sql_ast_node *node, int is_full);
void mylite_sql_ast_node_set_union_modifier(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_union_modifier modifier
);
void mylite_sql_ast_node_set_join_kind(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_join_kind join_kind
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
void mylite_sql_ast_node_set_varchar_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_varchar_type_payload payload
);
void mylite_sql_ast_node_set_char_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_char_type_payload payload
);
void mylite_sql_ast_node_set_text_type(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_text_type text_type
);
void mylite_sql_ast_node_set_binary_string_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_binary_string_type_payload payload
);
void mylite_sql_ast_node_set_bit_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_bit_type_payload payload
);
void mylite_sql_ast_node_set_year_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_year_type_payload payload
);
void mylite_sql_ast_node_set_decimal_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_decimal_type_payload payload
);
void mylite_sql_ast_node_set_approximate_type(
    struct mylite_sql_ast_node *node,
    struct mylite_sql_ast_approximate_type_payload payload
);
void mylite_sql_ast_node_set_nullability(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability nullability
);
void mylite_sql_ast_node_set_order_direction(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction direction
);
void mylite_sql_ast_node_set_column_visibility(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility visibility
);
void mylite_sql_ast_node_set_alter_table_options(
    struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);

size_t mylite_sql_ast_node_child_count(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_select_modifier mylite_sql_ast_node_select_modifier(
    const struct mylite_sql_ast_node *node
);
unsigned int mylite_sql_ast_node_select_options(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_select_calc_found_rows(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_select_locking_clause mylite_sql_ast_node_select_locking_clause(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_show_tables_is_full(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_union_modifier mylite_sql_ast_node_union_modifier(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_join_kind mylite_sql_ast_node_join_kind(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_literal_kind mylite_sql_ast_node_literal_kind(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_operator mylite_sql_ast_node_operator(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_integer_type mylite_sql_ast_node_integer_type(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_integer_type_is_unsigned(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_integer_type_has_display_width(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_integer_type_is_bool_alias(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_integer_type_is_serial_alias(const struct mylite_sql_ast_node *node);
struct mylite_sql_source_span mylite_sql_ast_node_integer_type_display_width_span(
    const struct mylite_sql_ast_node *node
);
struct mylite_sql_source_span mylite_sql_ast_node_varchar_type_length_span(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_varchar_type_is_national(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_char_type_has_explicit_length(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_char_type_is_national(const struct mylite_sql_ast_node *node);
struct mylite_sql_source_span mylite_sql_ast_node_char_type_length_span(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_text_type mylite_sql_ast_node_text_type(const struct mylite_sql_ast_node *node);
enum mylite_sql_ast_binary_string_type mylite_sql_ast_node_binary_string_type(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_binary_string_type_has_length(const struct mylite_sql_ast_node *node);
struct mylite_sql_source_span mylite_sql_ast_node_binary_string_type_length_span(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_bit_type_has_length(const struct mylite_sql_ast_node *node);
struct mylite_sql_source_span mylite_sql_ast_node_bit_type_length_span(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_year_type_has_width(const struct mylite_sql_ast_node *node);
struct mylite_sql_source_span mylite_sql_ast_node_year_type_width_span(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_decimal_type mylite_sql_ast_node_decimal_type(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_decimal_type_has_precision(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_decimal_type_has_scale(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_decimal_type_is_unsigned(const struct mylite_sql_ast_node *node);
struct mylite_sql_source_span mylite_sql_ast_node_decimal_type_precision_span(
    const struct mylite_sql_ast_node *node
);
struct mylite_sql_source_span mylite_sql_ast_node_decimal_type_scale_span(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_approximate_type mylite_sql_ast_node_approximate_type(
    const struct mylite_sql_ast_node *node
);
int mylite_sql_ast_node_approximate_type_has_precision(const struct mylite_sql_ast_node *node);
int mylite_sql_ast_node_approximate_type_is_unsigned(const struct mylite_sql_ast_node *node);
struct mylite_sql_source_span mylite_sql_ast_node_approximate_type_precision_span(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_nullability mylite_sql_ast_node_nullability(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_order_direction mylite_sql_ast_node_order_direction(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_column_visibility mylite_sql_ast_node_column_visibility(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_alter_algorithm mylite_sql_ast_node_alter_algorithm(
    const struct mylite_sql_ast_node *node
);
enum mylite_sql_ast_alter_lock mylite_sql_ast_node_alter_lock(
    const struct mylite_sql_ast_node *node
);

const char *mylite_sql_ast_node_kind_name(enum mylite_sql_ast_node_kind kind);
const char *mylite_sql_ast_literal_kind_name(enum mylite_sql_ast_literal_kind kind);
const char *mylite_sql_ast_operator_name(enum mylite_sql_ast_operator operator_kind);
const char *mylite_sql_ast_integer_type_name(enum mylite_sql_ast_integer_type integer_type);
const char *mylite_sql_ast_text_type_name(enum mylite_sql_ast_text_type text_type);
const char *mylite_sql_ast_binary_string_type_name(
    enum mylite_sql_ast_binary_string_type binary_string_type
);
const char *mylite_sql_ast_decimal_type_name(enum mylite_sql_ast_decimal_type decimal_type);
const char *mylite_sql_ast_approximate_type_name(
    enum mylite_sql_ast_approximate_type approximate_type
);
const char *mylite_sql_ast_nullability_name(enum mylite_sql_ast_nullability nullability);
const char *mylite_sql_ast_order_direction_name(enum mylite_sql_ast_order_direction direction);
const char *mylite_sql_ast_column_visibility_name(enum mylite_sql_ast_column_visibility visibility);

#endif
