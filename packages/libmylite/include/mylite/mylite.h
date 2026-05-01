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

MYLITE_API const char *mylite_version(void);
MYLITE_API const char *mylite_status_name(int status);

MYLITE_API int mylite_open(const char *filename, mylite_db **out_db);
MYLITE_API int mylite_open_memory(mylite_db **out_db);
MYLITE_API void mylite_close(mylite_db *database);
MYLITE_API const char *mylite_error_message(const mylite_db *database);

MYLITE_API int mylite_prepare(mylite_db *database, const char *sql, size_t length,
                              mylite_stmt **out_stmt);
MYLITE_API void mylite_finalize(mylite_stmt *stmt);
MYLITE_API int mylite_step(mylite_stmt *stmt);

MYLITE_API int mylite_column_count(const mylite_stmt *stmt);
MYLITE_API const char *mylite_column_name(const mylite_stmt *stmt, int column);
MYLITE_API int64_t mylite_column_int64(const mylite_stmt *stmt, int column);
MYLITE_API const char *mylite_column_text(const mylite_stmt *stmt, int column);

#ifdef __cplusplus
}
#endif

#endif
