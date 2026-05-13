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
