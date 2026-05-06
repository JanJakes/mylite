#ifndef MYLITE_MYLITE_H
#define MYLITE_MYLITE_H

#include <mylite/version.h>

#include <stddef.h>
#include <stdint.h>

#ifndef MYLITE_API
#  ifdef _WIN32
#    if defined(MYLITE_BUILDING_SHARED_LIBRARY)
#      define MYLITE_API __declspec(dllexport)
#    elif defined(MYLITE_USING_SHARED_LIBRARY)
#      define MYLITE_API __declspec(dllimport)
#    else
#      define MYLITE_API
#    endif
#  elif defined(__GNUC__) || defined(__clang__)
#    define MYLITE_API __attribute__((visibility("default")))
#  else
#    define MYLITE_API
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mylite_db mylite_db;
typedef struct mylite_stmt mylite_stmt;

enum mylite_status {
    MYLITE_OK = 0,
    MYLITE_MISUSE = 1,
    MYLITE_NOMEM = 2,
    MYLITE_PARSE_ERROR = 3,
    MYLITE_UNSUPPORTED = 4,
    MYLITE_SQLITE_ERROR = 5,
    MYLITE_EXEC_ERROR = 6,
    MYLITE_ROW = 100,
    MYLITE_DONE = 101,
};

enum mylite_field_type {
    MYLITE_FIELD_TYPE_INVALID = -1,
    MYLITE_FIELD_TYPE_DECIMAL = 0,
    MYLITE_FIELD_TYPE_TINY = 1,
    MYLITE_FIELD_TYPE_SHORT = 2,
    MYLITE_FIELD_TYPE_LONG = 3,
    MYLITE_FIELD_TYPE_FLOAT = 4,
    MYLITE_FIELD_TYPE_DOUBLE = 5,
    MYLITE_FIELD_TYPE_NULL = 6,
    MYLITE_FIELD_TYPE_TIMESTAMP = 7,
    MYLITE_FIELD_TYPE_LONGLONG = 8,
    MYLITE_FIELD_TYPE_INT24 = 9,
    MYLITE_FIELD_TYPE_DATE = 10,
    MYLITE_FIELD_TYPE_TIME = 11,
    MYLITE_FIELD_TYPE_DATETIME = 12,
    MYLITE_FIELD_TYPE_YEAR = 13,
    MYLITE_FIELD_TYPE_NEWDATE = 14,
    MYLITE_FIELD_TYPE_VARCHAR = 15,
    MYLITE_FIELD_TYPE_BIT = 16,
    MYLITE_FIELD_TYPE_JSON = 245,
    MYLITE_FIELD_TYPE_NEWDECIMAL = 246,
    MYLITE_FIELD_TYPE_ENUM = 247,
    MYLITE_FIELD_TYPE_SET = 248,
    MYLITE_FIELD_TYPE_TINY_BLOB = 249,
    MYLITE_FIELD_TYPE_MEDIUM_BLOB = 250,
    MYLITE_FIELD_TYPE_LONG_BLOB = 251,
    MYLITE_FIELD_TYPE_BLOB = 252,
    MYLITE_FIELD_TYPE_VAR_STRING = 253,
    MYLITE_FIELD_TYPE_STRING = 254,
    MYLITE_FIELD_TYPE_GEOMETRY = 255,
};

enum mylite_field_flag {
    MYLITE_FIELD_FLAG_NOT_NULL = 1U,
    MYLITE_FIELD_FLAG_PRI_KEY = 2U,
    MYLITE_FIELD_FLAG_UNIQUE_KEY = 4U,
    MYLITE_FIELD_FLAG_MULTIPLE_KEY = 8U,
    MYLITE_FIELD_FLAG_BLOB = 16U,
    MYLITE_FIELD_FLAG_UNSIGNED = 32U,
    MYLITE_FIELD_FLAG_ZEROFILL = 64U,
    MYLITE_FIELD_FLAG_BINARY = 128U,
    MYLITE_FIELD_FLAG_ENUM = 256U,
    MYLITE_FIELD_FLAG_AUTO_INCREMENT = 512U,
    MYLITE_FIELD_FLAG_TIMESTAMP = 1024U,
    MYLITE_FIELD_FLAG_SET = 2048U,
    MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE = 4096U,
    MYLITE_FIELD_FLAG_ON_UPDATE_NOW = 8192U,
    MYLITE_FIELD_FLAG_PART_KEY = 16384U,
    MYLITE_FIELD_FLAG_NUM = 32768U,
};

MYLITE_API const char *mylite_version(void);
MYLITE_API const char *mylite_status_name(int status);

MYLITE_API int mylite_open(const char *filename, mylite_db **out_db);
MYLITE_API int mylite_open_memory(mylite_db **out_db);
MYLITE_API void mylite_close(mylite_db *database);
MYLITE_API const char *mylite_error_message(const mylite_db *database);

MYLITE_API int mylite_prepare(
    mylite_db *database,
    const char *sql,
    size_t length,
    mylite_stmt **out_stmt
);
MYLITE_API void mylite_finalize(mylite_stmt *stmt);
MYLITE_API int mylite_step(mylite_stmt *stmt);
MYLITE_API int64_t mylite_affected_rows(const mylite_stmt *stmt);
MYLITE_API uint64_t mylite_last_insert_id(const mylite_db *database);
MYLITE_API int mylite_warning_count(const mylite_db *database);
MYLITE_API unsigned int mylite_warning_code(const mylite_db *database, int warning);
MYLITE_API const char *mylite_warning_message(const mylite_db *database, int warning);

MYLITE_API int mylite_column_count(const mylite_stmt *stmt);
MYLITE_API const char *mylite_column_name(const mylite_stmt *stmt, int column);
MYLITE_API const char *mylite_column_schema_name(const mylite_stmt *stmt, int column);
MYLITE_API const char *mylite_column_table_name(const mylite_stmt *stmt, int column);
MYLITE_API const char *mylite_column_origin_schema_name(const mylite_stmt *stmt, int column);
MYLITE_API const char *mylite_column_origin_table_name(const mylite_stmt *stmt, int column);
MYLITE_API const char *mylite_column_origin_name(const mylite_stmt *stmt, int column);
MYLITE_API int mylite_column_field_type(const mylite_stmt *stmt, int column);
MYLITE_API unsigned int mylite_column_flags(const mylite_stmt *stmt, int column);
MYLITE_API uint64_t mylite_column_declared_length(const mylite_stmt *stmt, int column);
MYLITE_API uint64_t mylite_column_max_length(const mylite_stmt *stmt, int column);
MYLITE_API unsigned int mylite_column_decimals(const mylite_stmt *stmt, int column);
MYLITE_API unsigned int mylite_column_charset_id(const mylite_stmt *stmt, int column);
MYLITE_API int mylite_column_is_nullable(const mylite_stmt *stmt, int column);
MYLITE_API int64_t mylite_column_int64(const mylite_stmt *stmt, int column);
MYLITE_API const char *mylite_column_text(const mylite_stmt *stmt, int column);
MYLITE_API uint64_t mylite_column_bytes(const mylite_stmt *stmt, int column);

#ifdef __cplusplus
}
#endif

#endif
