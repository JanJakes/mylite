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
typedef struct mylite_result mylite_result;

#define MYLITE_OK 0
#define MYLITE_ERROR 1
#define MYLITE_NOMEM 7
#define MYLITE_MISUSE 21

enum mylite_result_column_type {
    MYLITE_RESULT_COLUMN_TYPE_UNKNOWN = -1,
    MYLITE_RESULT_COLUMN_TYPE_DECIMAL = 0,
    MYLITE_RESULT_COLUMN_TYPE_TINY = 1,
    MYLITE_RESULT_COLUMN_TYPE_SHORT = 2,
    MYLITE_RESULT_COLUMN_TYPE_LONG = 3,
    MYLITE_RESULT_COLUMN_TYPE_FLOAT = 4,
    MYLITE_RESULT_COLUMN_TYPE_DOUBLE = 5,
    MYLITE_RESULT_COLUMN_TYPE_NULL = 6,
    MYLITE_RESULT_COLUMN_TYPE_TIMESTAMP = 7,
    MYLITE_RESULT_COLUMN_TYPE_LONGLONG = 8,
    MYLITE_RESULT_COLUMN_TYPE_INT24 = 9,
    MYLITE_RESULT_COLUMN_TYPE_DATE = 10,
    MYLITE_RESULT_COLUMN_TYPE_TIME = 11,
    MYLITE_RESULT_COLUMN_TYPE_DATETIME = 12,
    MYLITE_RESULT_COLUMN_TYPE_YEAR = 13,
    MYLITE_RESULT_COLUMN_TYPE_VARCHAR = 15,
    MYLITE_RESULT_COLUMN_TYPE_BIT = 16,
    MYLITE_RESULT_COLUMN_TYPE_JSON = 245,
    MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL = 246,
    MYLITE_RESULT_COLUMN_TYPE_BLOB = 252,
    MYLITE_RESULT_COLUMN_TYPE_VAR_STRING = 253,
    MYLITE_RESULT_COLUMN_TYPE_STRING = 254,
    MYLITE_RESULT_COLUMN_TYPE_GEOMETRY = 255,
};

#define MYLITE_RESULT_COLUMN_FLAG_NOT_NULL 1u
#define MYLITE_RESULT_COLUMN_FLAG_PRI_KEY 2u
#define MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY 4u
#define MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY 8u
#define MYLITE_RESULT_COLUMN_FLAG_BLOB 16u
#define MYLITE_RESULT_COLUMN_FLAG_UNSIGNED 32u
#define MYLITE_RESULT_COLUMN_FLAG_ZEROFILL 64u
#define MYLITE_RESULT_COLUMN_FLAG_BINARY 128u
#define MYLITE_RESULT_COLUMN_FLAG_ENUM 256u
#define MYLITE_RESULT_COLUMN_FLAG_AUTO_INCREMENT 512u
#define MYLITE_RESULT_COLUMN_FLAG_SET 2048u
#define MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT 4096u
#define MYLITE_RESULT_COLUMN_FLAG_PART_KEY 16384u
#define MYLITE_RESULT_COLUMN_FLAG_NUM 32768u

MYLITE_API const char *mylite_version(void);

MYLITE_API int mylite_open(const char *path, mylite_db **out_db);
MYLITE_API int mylite_open_memory(mylite_db **out_db);
MYLITE_API void mylite_close(mylite_db *database);

MYLITE_API int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
);

MYLITE_API void mylite_result_free(mylite_result *result);
MYLITE_API size_t mylite_result_column_count(const mylite_result *result);
MYLITE_API const char *mylite_result_column_name(const mylite_result *result, size_t column_index);
MYLITE_API const char *mylite_result_column_schema_name(
    const mylite_result *result,
    size_t column_index
);
MYLITE_API const char *mylite_result_column_table_name(
    const mylite_result *result,
    size_t column_index
);
MYLITE_API const char *mylite_result_column_origin_schema_name(
    const mylite_result *result,
    size_t column_index
);
MYLITE_API const char *mylite_result_column_origin_table_name(
    const mylite_result *result,
    size_t column_index
);
MYLITE_API const char *mylite_result_column_origin_name(
    const mylite_result *result,
    size_t column_index
);
MYLITE_API enum mylite_result_column_type mylite_result_column_type(
    const mylite_result *result,
    size_t column_index
);
MYLITE_API uint32_t mylite_result_column_flags(const mylite_result *result, size_t column_index);
MYLITE_API uint32_t
mylite_result_column_charset_id(const mylite_result *result, size_t column_index);
MYLITE_API uint32_t
mylite_result_column_collation_id(const mylite_result *result, size_t column_index);
MYLITE_API uint64_t
mylite_result_column_display_length(const mylite_result *result, size_t column_index);
MYLITE_API uint16_t mylite_result_column_decimals(const mylite_result *result, size_t column_index);
MYLITE_API int mylite_result_column_nullable(const mylite_result *result, size_t column_index);
MYLITE_API size_t mylite_result_row_count(const mylite_result *result);
MYLITE_API const char *mylite_result_value_text(
    const mylite_result *result,
    size_t row_index,
    size_t column_index
);
MYLITE_API const void *mylite_result_value_bytes(
    const mylite_result *result,
    size_t row_index,
    size_t column_index
);
MYLITE_API size_t
mylite_result_value_size(const mylite_result *result, size_t row_index, size_t column_index);
MYLITE_API int64_t mylite_result_affected_rows(const mylite_result *result);
MYLITE_API size_t mylite_result_warning_count(const mylite_result *result);

MYLITE_API int mylite_errcode(const mylite_db *database);
MYLITE_API const char *mylite_sqlstate(const mylite_db *database);
MYLITE_API const char *mylite_errmsg(const mylite_db *database);

#ifdef __cplusplus
}
#endif

#endif
