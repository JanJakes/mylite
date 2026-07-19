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
typedef struct mylite_stmt mylite_stmt;

enum {
    MYLITE_OPEN_DIAGNOSTIC_SQLSTATE_CAPACITY = 6,
    MYLITE_OPEN_DIAGNOSTIC_MESSAGE_CAPACITY = 256,
};

struct mylite_open_diagnostic {
    int error_code;
    char sqlstate[MYLITE_OPEN_DIAGNOSTIC_SQLSTATE_CAPACITY];
    char message[MYLITE_OPEN_DIAGNOSTIC_MESSAGE_CAPACITY];
};

#define MYLITE_OK 0
#define MYLITE_ERROR 1
#define MYLITE_NOMEM 7
#define MYLITE_MISUSE 21
#define MYLITE_ROW 100
#define MYLITE_DONE 101

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

enum mylite_transaction_control_statement {
    MYLITE_TRANSACTION_CONTROL_START = 1,
    MYLITE_TRANSACTION_CONTROL_COMMIT = 2,
    MYLITE_TRANSACTION_CONTROL_ROLLBACK = 3,
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

/*
 * A database handle and all statements created from it are single-threaded.
 * Different database handles may be used concurrently. Callbacks into the same
 * handle are not reentrant.
 *
 * On success, open stores a caller-owned handle in out_db. The path and SQL
 * inputs are copied when they must outlive the call.
 */
MYLITE_API int mylite_open(const char *path, mylite_db **out_db);
MYLITE_API int mylite_open_memory(mylite_db **out_db);
/*
 * Diagnostic open variants populate out_diagnostic on both success and
 * failure. The record is caller-owned and remains valid independently of the
 * database handle. Passing NULL for out_diagnostic is allowed.
 */
MYLITE_API int mylite_open_with_diagnostic(
    const char *path,
    mylite_db **out_db,
    struct mylite_open_diagnostic *out_diagnostic
);
MYLITE_API int mylite_open_memory_with_diagnostic(
    mylite_db **out_db,
    struct mylite_open_diagnostic *out_diagnostic
);
/*
 * Close always consumes the database handle. Live statements are detached;
 * every statement operation remains memory-safe, but only finalize is valid.
 */
MYLITE_API void mylite_close(mylite_db *database);
/*
 * Checked close consumes the handle on success. On failure, the database
 * remains caller-owned and may be closed again, but live statements have been
 * detached and are valid only for finalize.
 */
MYLITE_API int mylite_close_checked(mylite_db *database);

/* On success, out_result is caller-owned and must be released with result_free. */
MYLITE_API int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
);
MYLITE_API int mylite_execute_transaction_control(
    mylite_db *database,
    enum mylite_transaction_control_statement statement,
    mylite_result **out_result
);

MYLITE_API int mylite_prepare(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_stmt **out_stmt
);
/*
 * Prepared statements are caller-owned until finalize. They retain their own
 * SQL text and are registered with the database until finalized or detached by
 * database close. Buffered statements release the connection before rows are
 * consumed.
 */
MYLITE_API int mylite_prepare_buffered(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_stmt **out_stmt
);
/* Parameter indexes are zero-based. Text and blob bindings are copied. */
MYLITE_API size_t mylite_stmt_parameter_count(const mylite_stmt *stmt);
MYLITE_API int mylite_stmt_bind_null(mylite_stmt *stmt, size_t index);
MYLITE_API int mylite_stmt_bind_int64(mylite_stmt *stmt, size_t index, int64_t value);
MYLITE_API int mylite_stmt_bind_uint64(mylite_stmt *stmt, size_t index, uint64_t value);
MYLITE_API int mylite_stmt_bind_double(mylite_stmt *stmt, size_t index, double value);
MYLITE_API int mylite_stmt_bind_text(
    mylite_stmt *stmt,
    size_t index,
    const char *value,
    size_t value_size
);
MYLITE_API int mylite_stmt_bind_blob(
    mylite_stmt *stmt,
    size_t index,
    const void *value,
    size_t value_size
);
MYLITE_API int mylite_stmt_clear_bindings(mylite_stmt *stmt);
/* Reset preserves bindings and makes a completed or failed statement executable again. */
MYLITE_API int mylite_stmt_reset(mylite_stmt *stmt);
MYLITE_API int64_t mylite_stmt_affected_rows(const mylite_stmt *stmt);
MYLITE_API uint64_t mylite_stmt_insert_id(const mylite_stmt *stmt);
MYLITE_API int mylite_stmt_step(mylite_stmt *stmt);
MYLITE_API int mylite_stmt_finalize(mylite_stmt *stmt);
MYLITE_API size_t mylite_stmt_column_count(const mylite_stmt *stmt);
MYLITE_API const char *mylite_stmt_column_name(const mylite_stmt *stmt, size_t column_index);
MYLITE_API const char *mylite_stmt_column_schema_name(const mylite_stmt *stmt, size_t column_index);
MYLITE_API const char *mylite_stmt_column_table_name(const mylite_stmt *stmt, size_t column_index);
MYLITE_API const char *mylite_stmt_column_origin_schema_name(
    const mylite_stmt *stmt,
    size_t column_index
);
MYLITE_API const char *mylite_stmt_column_origin_table_name(
    const mylite_stmt *stmt,
    size_t column_index
);
MYLITE_API const char *mylite_stmt_column_origin_name(const mylite_stmt *stmt, size_t column_index);
MYLITE_API enum mylite_result_column_type mylite_stmt_column_type(
    const mylite_stmt *stmt,
    size_t column_index
);
MYLITE_API uint32_t mylite_stmt_column_flags(const mylite_stmt *stmt, size_t column_index);
MYLITE_API uint32_t mylite_stmt_column_charset_id(const mylite_stmt *stmt, size_t column_index);
MYLITE_API uint32_t mylite_stmt_column_collation_id(const mylite_stmt *stmt, size_t column_index);
MYLITE_API uint64_t mylite_stmt_column_display_length(const mylite_stmt *stmt, size_t column_index);
MYLITE_API uint16_t mylite_stmt_column_decimals(const mylite_stmt *stmt, size_t column_index);
MYLITE_API int mylite_stmt_column_nullable(const mylite_stmt *stmt, size_t column_index);
/*
 * Column metadata pointers are borrowed from the statement and remain valid
 * until reset, finalize, or database close. Value access is valid only after
 * step returns MYLITE_ROW. Value pointers remain valid until the next step,
 * reset, finalize, or database close. SQL NULL is reported by value_is_null;
 * empty text/blob is non-NULL, has size zero, and has a non-NULL bytes pointer.
 */
MYLITE_API int mylite_stmt_value_is_null(const mylite_stmt *stmt, size_t column_index);
MYLITE_API const char *mylite_stmt_value_text(const mylite_stmt *stmt, size_t column_index);
MYLITE_API const void *mylite_stmt_value_bytes(const mylite_stmt *stmt, size_t column_index);
MYLITE_API size_t mylite_stmt_value_size(const mylite_stmt *stmt, size_t column_index);

/* Results are independent of the database and remain valid until result_free. */
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
/*
 * Result metadata and value pointers are borrowed from the result and remain
 * valid until result_free. SQL NULL is reported by value_is_null; empty
 * text/blob is non-NULL, has size zero, and has a non-NULL bytes pointer.
 */
MYLITE_API int mylite_result_value_is_null(
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
MYLITE_API const char *mylite_result_info(const mylite_result *result);
MYLITE_API uint64_t mylite_result_insert_id(const mylite_result *result);
MYLITE_API size_t mylite_result_warning_count(const mylite_result *result);

/* Returns 1 when enabled, 0 when disabled, and -1 for an invalid handle. */
MYLITE_API int mylite_session_no_backslash_escapes(const mylite_db *database);
MYLITE_API int mylite_errcode(const mylite_db *database);
/* SQLSTATE and message pointers remain valid until the next database API call. */
MYLITE_API const char *mylite_sqlstate(const mylite_db *database);
MYLITE_API const char *mylite_errmsg(const mylite_db *database);

#ifdef __cplusplus
}
#endif

#endif
